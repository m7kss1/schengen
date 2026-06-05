use std::ffi::{CStr, CString, c_char};
use std::future::ready;
use std::fs::OpenOptions;
use std::io::{self, BufWriter, Write};
use std::path::PathBuf;
use std::ptr;
use std::sync::{Arc, Mutex, OnceLock};
use std::thread::{self, JoinHandle};

use arrow_array::ffi::{FFI_ArrowArray, from_ffi_and_data_type};
use arrow_array::StructArray;
use arrow_schema::ffi::FFI_ArrowSchema;
use arrow_schema::{DataType, Schema as ArrowSchema, SchemaRef};
use futures::channel::mpsc::{Receiver, Sender, channel};
use futures::{SinkExt, StreamExt};
use object_store::aws::AmazonS3Builder;
use object_store::azure::MicrosoftAzureBuilder;
use object_store::client::SpawnedReqwestConnector;
use object_store::gcp::GoogleCloudStorageBuilder;
use object_store::local::LocalFileSystem;
use object_store::path::Path as ObjectStorePath;
use object_store::{ObjectStore, ObjectStoreExt, ObjectStoreScheme};
use tokio::runtime::{Builder as TokioRuntimeBuilder, Runtime as TokioRuntime};
use url::Url;
use vortex::VortexSessionDefault;
use vortex::array::ArrayRef;
use vortex::array::arrow::FromArrowArray;
use vortex::array::stream::ArrayStreamAdapter;
use vortex::dtype::DType;
use vortex::dtype::arrow::FromArrowType;
use vortex::file::{OpenOptionsSessionExt, WriteOptionsSessionExt, WriteStrategyBuilder};
use vortex::io::{IoBuf, VortexWrite};
use vortex::io::object_store::ObjectStoreWrite;
use vortex::io::runtime::BlockingRuntime;
use vortex::io::runtime::current::CurrentThreadRuntime;
use vortex::io::session::RuntimeSessionExt;
use vortex::session::VortexSession;

#[repr(C)]
pub struct VortexWriterOptionsC {
    pub abi_version: u32,
    pub reserved: u32,
    pub row_block_size: i64,
    pub output_buffer_bytes: i64,
}

#[repr(C)]
pub struct VortexFileInfoC {
    pub row_count: i64,
    pub field_names_csv: *mut c_char,
}

static LAST_ERROR: OnceLock<Mutex<CString>> = OnceLock::new();
static TOKIO_RUNTIME: OnceLock<TokioRuntime> = OnceLock::new();

enum OutputTarget {
    Local(PathBuf),
    ObjectStore {
        url: Url,
        path: ObjectStorePath,
    },
}

#[derive(Clone, Copy)]
struct WriterConfig {
    row_block_size: usize,
    output_buffer_bytes: usize,
}

struct StdIoWriteAdapter<W>(W);

impl<W: Write + Unpin> VortexWrite for StdIoWriteAdapter<W> {
    async fn write_all<B: IoBuf>(&mut self, buffer: B) -> io::Result<B> {
        self.0.write_all(buffer.as_slice())?;
        Ok(buffer)
    }

    fn flush(&mut self) -> impl std::future::Future<Output = io::Result<()>> {
        ready(self.0.flush())
    }

    fn shutdown(&mut self) -> impl std::future::Future<Output = io::Result<()>> {
        ready(Ok(()))
    }
}

pub struct VortexWriterHandle {
    schema: SchemaRef,
    sender: Option<Sender<ArrayRef>>,
    worker: Option<JoinHandle<Result<(), String>>>,
    last_error: CString,
    finished: bool,
}

impl VortexWriterHandle {
    fn new(
        schema: SchemaRef,
        sender: Sender<ArrayRef>,
        worker: JoinHandle<Result<(), String>>,
    ) -> Self {
        Self {
            schema,
            sender: Some(sender),
            worker: Some(worker),
            last_error: CString::new("").unwrap(),
            finished: false,
        }
    }

    fn set_error(&mut self, message: impl Into<String>) -> i32 {
        self.last_error = sanitize_message(message);
        1
    }

    fn clear_error(&mut self) {
        self.last_error = CString::new("").unwrap();
    }
}

fn global_error_slot() -> &'static Mutex<CString> {
    LAST_ERROR.get_or_init(|| Mutex::new(CString::new("").unwrap()))
}

fn sanitize_message(message: impl Into<String>) -> CString {
    let message = message.into().replace('\0', " ");
    CString::new(message).unwrap_or_else(|_| CString::new("unknown vortex bridge error").unwrap())
}

fn set_global_error(message: impl Into<String>) -> i32 {
    if let Ok(mut guard) = global_error_slot().lock() {
        *guard = sanitize_message(message);
    }
    1
}

fn clear_global_error() {
    if let Ok(mut guard) = global_error_slot().lock() {
        *guard = CString::new("").unwrap();
    }
}

fn c_string_to_owned(raw: *const c_char, field_name: &str) -> Result<String, String> {
    if raw.is_null() {
        return Err(format!("{field_name} must not be null"));
    }
    unsafe { CStr::from_ptr(raw) }
        .to_str()
        .map(|value| value.to_owned())
        .map_err(|err| err.to_string())
}

fn schema_from_raw(schema: *mut FFI_ArrowSchema) -> Result<SchemaRef, String> {
    if schema.is_null() {
        return Err("schema must not be null".to_string());
    }
    let ffi_schema = unsafe { FFI_ArrowSchema::from_raw(schema) };
    let schema = ArrowSchema::try_from(&ffi_schema).map_err(|err| err.to_string())?;
    Ok(Arc::new(schema))
}

fn array_from_raw(array: *mut FFI_ArrowArray, schema: &SchemaRef) -> Result<ArrayRef, String> {
    if array.is_null() {
        return Err("array must not be null".to_string());
    }
    let ffi_array = unsafe { ptr::read(array) };
    let data = unsafe { from_ffi_and_data_type(ffi_array, DataType::Struct(schema.fields.clone())) }
        .map_err(|err| err.to_string())?;
    let struct_array = StructArray::from(data);
    ArrayRef::from_arrow(&struct_array, false).map_err(|err| err.to_string())
}

fn build_dtype(schema: &SchemaRef) -> DType {
    DType::from_arrow(schema.clone())
}

fn parse_positive_usize(value: i64, field_name: &str) -> Result<usize, String> {
    if value <= 0 {
        return Err(format!("{field_name} must be > 0"));
    }
    usize::try_from(value).map_err(|_| format!("{field_name} is out of range"))
}

fn parse_writer_options(options: *const VortexWriterOptionsC) -> Result<WriterConfig, String> {
    if options.is_null() {
        return Err("options must not be null".to_string());
    }
    let options = unsafe { &*options };
    if options.abi_version != 2 {
        return Err(format!("unsupported Vortex bridge ABI version: {}", options.abi_version));
    }
    Ok(WriterConfig {
        row_block_size: parse_positive_usize(options.row_block_size, "row_block_size")?,
        output_buffer_bytes: parse_positive_usize(options.output_buffer_bytes, "output_buffer_bytes")?,
    })
}

fn tokio_runtime() -> Result<&'static TokioRuntime, String> {
    if let Some(runtime) = TOKIO_RUNTIME.get() {
        return Ok(runtime);
    }

    let runtime = TokioRuntimeBuilder::new_multi_thread()
        .enable_all()
        .build()
        .map_err(|err| err.to_string())?;

    let _ = TOKIO_RUNTIME.set(runtime);
    TOKIO_RUNTIME
        .get()
        .ok_or_else(|| "failed to initialize Tokio runtime".to_string())
}

fn spawned_reqwest_connector() -> Result<SpawnedReqwestConnector, String> {
    Ok(SpawnedReqwestConnector::new(tokio_runtime()?.handle().clone()))
}

fn make_object_store(url: &Url) -> Result<Arc<dyn ObjectStore>, String> {
    let (scheme, _) = ObjectStoreScheme::parse(url).map_err(|err| err.to_string())?;
    match scheme {
        ObjectStoreScheme::Local => Ok(Arc::new(LocalFileSystem::default())),
        ObjectStoreScheme::AmazonS3 => {
            let mut builder = AmazonS3Builder::from_env()
                .with_http_connector(spawned_reqwest_connector()?)
                .with_url(url.to_string());
            if let Some(bucket) = url.domain() {
                builder = builder.with_bucket_name(bucket);
            }
            builder
                .build()
                .map(|store| Arc::new(store) as Arc<dyn ObjectStore>)
                .map_err(|err| err.to_string())
        }
        ObjectStoreScheme::MicrosoftAzure => MicrosoftAzureBuilder::new()
            .with_http_connector(spawned_reqwest_connector()?)
            .with_url(url.to_string())
            .build()
            .map(|store| Arc::new(store) as Arc<dyn ObjectStore>)
            .map_err(|err| err.to_string()),
        ObjectStoreScheme::GoogleCloudStorage => GoogleCloudStorageBuilder::new()
            .with_http_connector(spawned_reqwest_connector()?)
            .with_url(url.to_string())
            .build()
            .map(|store| Arc::new(store) as Arc<dyn ObjectStore>)
            .map_err(|err| err.to_string()),
        other => Err(format!("unsupported object store scheme: {other:?}")),
    }
}

fn resolve_target(uri: &str) -> Result<OutputTarget, String> {
    match Url::parse(uri) {
        Ok(url) => {
            if url.scheme() == "file" {
                let path = url
                    .to_file_path()
                    .map_err(|_| format!("invalid file URI: {uri}"))?;
                Ok(OutputTarget::Local(path))
            } else {
                let path = ObjectStorePath::from_url_path(url.path())
                    .map_err(|_| format!("invalid object store path: {}", url.path()))?;
                Ok(OutputTarget::ObjectStore { url, path })
            }
        }
        Err(url::ParseError::RelativeUrlWithoutBase) => Ok(OutputTarget::Local(PathBuf::from(uri))),
        Err(err) => Err(format!("failed to parse uri '{uri}': {err}")),
    }
}

fn ensure_create_target(target: &OutputTarget) -> Result<(), String> {
    match target {
        OutputTarget::Local(path) => {
            if path.exists() {
                return Err(format!("file already exists: {}", path.display()));
            }
            if let Some(parent) = path.parent()
                && !parent.as_os_str().is_empty()
            {
                std::fs::create_dir_all(parent).map_err(|err| err.to_string())?;
            }
            Ok(())
        }
        OutputTarget::ObjectStore { url, path } => {
            let runtime = CurrentThreadRuntime::new();
            runtime.block_on(async {
                let store = make_object_store(url)?;
                match store.head(path).await {
                    Ok(_) => Err(format!("file already exists: {path}")),
                    Err(object_store::Error::NotFound { .. }) => Ok(()),
                    Err(err) => Err(err.to_string()),
                }
            })
        }
    }
}

fn session_for_runtime(runtime: &CurrentThreadRuntime) -> VortexSession {
    VortexSession::default().with_handle(runtime.handle())
}

fn build_write_options(session: &VortexSession, config: WriterConfig) -> vortex::file::VortexWriteOptions {
    let strategy = WriteStrategyBuilder::default()
        .with_row_block_size(config.row_block_size)
        .build();
    session.write_options().with_strategy(strategy)
}

fn write_batches(
    target: OutputTarget,
    dtype: DType,
    config: WriterConfig,
    receiver: Receiver<ArrayRef>,
) -> Result<(), String> {
    let runtime = CurrentThreadRuntime::new();

    // Drive Vortex's spawn_cpu compression tasks across all cores. The pool shares the
    // runtime's executor, so CompressingStrategy's `.buffered(concurrency)` chunks compress
    // in parallel while the main write future and the sync file sink stay on this thread.
    // Output ordering is preserved by Vortex (sequence IDs + buffered()). The pool must
    // outlive block_on; its Drop signals the workers to stop.
    let _pool = {
        let pool = runtime.new_pool();
        pool.set_workers_to_available_parallelism();
        pool
    };

    let session = session_for_runtime(&runtime);

    runtime.block_on(async move {
        let stream = receiver.map(Ok);
        let array_stream = ArrayStreamAdapter::new(dtype, stream);
        let write_options = build_write_options(&session, config);

        match target {
            OutputTarget::Local(path) => {
                let file = OpenOptions::new()
                    .write(true)
                    .create_new(true)
                    .open(&path)
                    .map_err(|err| err.to_string())?;
                let sink = StdIoWriteAdapter(BufWriter::with_capacity(config.output_buffer_bytes, file));
                write_options
                    .write(sink, array_stream)
                    .await
                    .map_err(|err| err.to_string())?;
            }
            OutputTarget::ObjectStore { url, path } => {
                let store = make_object_store(&url)?;
                let mut writer = ObjectStoreWrite::new(store, &path)
                    .await
                    .map_err(|err| err.to_string())?;
                write_options
                    .write(&mut writer, array_stream)
                    .await
                    .map_err(|err| err.to_string())?;
                VortexWrite::shutdown(&mut writer)
                    .await
                    .map_err(|err| err.to_string())?;
            }
        }

        Ok(())
    })
}

fn inspect_target(target: OutputTarget) -> Result<(i64, String), String> {
    let runtime = CurrentThreadRuntime::new();
    let session = session_for_runtime(&runtime);

    runtime.block_on(async move {
        let file = match target {
            OutputTarget::Local(path) => session
                .open_options()
                .open_path(path.as_path())
                .await
                .map_err(|err| err.to_string())?,
            OutputTarget::ObjectStore { url, .. } => {
                let store = make_object_store(&url)?;
                session
                .open_options()
                .open_object_store(&store, url.path())
                .await
                .map_err(|err| err.to_string())?
            }
        };

        let row_count = i64::try_from(file.row_count()).map_err(|err| err.to_string())?;
        let schema = file.dtype().to_arrow_schema().map_err(|err| err.to_string())?;
        let field_names_csv = schema
            .fields()
            .iter()
            .map(|field| field.name().as_str())
            .collect::<Vec<_>>()
            .join(",");
        Ok((row_count, field_names_csv))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn vortex_writer_open(
    uri: *const c_char,
    schema: *mut FFI_ArrowSchema,
    options: *const VortexWriterOptionsC,
    out_handle: *mut *mut VortexWriterHandle,
) -> i32 {
    clear_global_error();

    let result = (|| -> Result<*mut VortexWriterHandle, String> {
        if out_handle.is_null() {
            return Err("out_handle must not be null".to_string());
        }

        let config = parse_writer_options(options)?;

        let uri = c_string_to_owned(uri, "uri")?;
        let schema = schema_from_raw(schema)?;
        let dtype = build_dtype(&schema);
        let target = resolve_target(uri.as_str())?;
        ensure_create_target(&target)?;

        let (sender, receiver) = channel::<ArrayRef>(8);
        let worker_dtype = dtype.clone();
        let worker = thread::spawn(move || write_batches(target, worker_dtype, config, receiver));

        Ok(Box::into_raw(Box::new(VortexWriterHandle::new(schema, sender, worker))))
    })();

    match result {
        Ok(handle) => {
            unsafe { *out_handle = handle };
            0
        }
        Err(err) => set_global_error(err),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn vortex_writer_begin_partition(
    handle: *mut VortexWriterHandle,
    _part_num: i32,
    _part_count: i32,
) -> i32 {
    if handle.is_null() {
        return set_global_error("handle must not be null");
    }

    let handle = unsafe { &mut *handle };
    handle.clear_error();
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn vortex_writer_push_batch(
    handle: *mut VortexWriterHandle,
    array: *mut FFI_ArrowArray,
) -> i32 {
    if handle.is_null() {
        return set_global_error("handle must not be null");
    }

    let handle = unsafe { &mut *handle };
    handle.clear_error();

    if handle.finished {
        return handle.set_error("writer is already finished");
    }

    let batch = match array_from_raw(array, &handle.schema) {
        Ok(batch) => batch,
        Err(err) => return handle.set_error(err),
    };

    match handle.sender.as_mut() {
        Some(sender) => futures::executor::block_on(sender.send(batch))
            .map(|_| 0)
            .unwrap_or_else(|err| handle.set_error(err.to_string())),
        None => handle.set_error("writer channel is closed"),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn vortex_writer_end_partition(handle: *mut VortexWriterHandle) -> i32 {
    if handle.is_null() {
        return set_global_error("handle must not be null");
    }

    let handle = unsafe { &mut *handle };
    handle.clear_error();
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn vortex_writer_finish(handle: *mut VortexWriterHandle) -> i32 {
    if handle.is_null() {
        return set_global_error("handle must not be null");
    }

    let handle = unsafe { &mut *handle };
    handle.clear_error();

    if handle.finished {
        return 0;
    }

    handle.sender.take();

    if let Some(worker) = handle.worker.take() {
        match worker.join() {
            Ok(Ok(())) => {
                handle.finished = true;
                0
            }
            Ok(Err(err)) => handle.set_error(err),
            Err(_) => handle.set_error("vortex writer thread panicked"),
        }
    } else {
        handle.finished = true;
        0
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn vortex_writer_destroy(handle: *mut VortexWriterHandle) {
    if handle.is_null() {
        return;
    }

    let mut handle = unsafe { Box::from_raw(handle) };
    if !handle.finished {
        let _ = vortex_writer_finish(&mut *handle);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn vortex_last_error(handle: *const VortexWriterHandle) -> *const c_char {
    if handle.is_null() {
        return vortex_bridge_last_error();
    }
    let handle = unsafe { &*handle };
    handle.last_error.as_ptr()
}

#[unsafe(no_mangle)]
pub extern "C" fn vortex_bridge_last_error() -> *const c_char {
    match global_error_slot().lock() {
        Ok(guard) => guard.as_ptr(),
        Err(_) => c"failed to acquire bridge error lock".as_ptr(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn vortex_file_inspect(uri: *const c_char, out_info: *mut VortexFileInfoC) -> i32 {
    clear_global_error();

    let result = (|| -> Result<(), String> {
        if out_info.is_null() {
            return Err("out_info must not be null".to_string());
        }

        let uri = c_string_to_owned(uri, "uri")?;
        let target = resolve_target(uri.as_str())?;
        let (row_count, field_names_csv) = inspect_target(target)?;

        unsafe {
            (*out_info).row_count = row_count;
            (*out_info).field_names_csv = sanitize_message(field_names_csv).into_raw();
        }
        Ok(())
    })();

    match result {
        Ok(()) => 0,
        Err(err) => set_global_error(err),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn vortex_file_info_destroy(info: *mut VortexFileInfoC) {
    if info.is_null() {
        return;
    }

    unsafe {
        if !(*info).field_names_csv.is_null() {
            let _ = CString::from_raw((*info).field_names_csv);
            (*info).field_names_csv = ptr::null_mut();
        }
        (*info).row_count = 0;
    }
}
