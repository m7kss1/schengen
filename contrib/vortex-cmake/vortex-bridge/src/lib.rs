use std::ffi::{CStr, CString, c_char};
use std::path::PathBuf;
use std::ptr;
use std::sync::{Arc, Mutex, OnceLock};
use std::thread::{self, JoinHandle};

use arrow_array::ffi::{FFI_ArrowArray, from_ffi_and_data_type};
use arrow_array::{RecordBatch, StructArray};
use arrow_schema::ffi::FFI_ArrowSchema;
use arrow_schema::{DataType, Schema as ArrowSchema, SchemaRef};
use futures::channel::mpsc::{Receiver, Sender, channel};
use futures::{SinkExt, StreamExt};
use object_store::aws::AmazonS3Builder;
use object_store::azure::MicrosoftAzureBuilder;
use object_store::gcp::GoogleCloudStorageBuilder;
use object_store::local::LocalFileSystem;
use object_store::path::Path as ObjectStorePath;
use object_store::{ObjectStore, ObjectStoreScheme};
use url::Url;
use vortex::VortexSessionDefault;
use vortex::array::ArrayRef;
use vortex::array::arrow::FromArrowArray;
use vortex::array::stream::ArrayStreamAdapter;
use vortex::dtype::DType;
use vortex::dtype::arrow::FromArrowType;
use vortex::file::{OpenOptionsSessionExt, WriteOptionsSessionExt};
use vortex::io::VortexWrite;
use vortex::io::object_store::ObjectStoreWrite;
use vortex::io::runtime::BlockingRuntime;
use vortex::io::runtime::current::CurrentThreadRuntime;
use vortex::io::session::RuntimeSessionExt;
use vortex::session::VortexSession;

#[repr(C)]
pub struct VortexWriterOptionsC {
    pub abi_version: u32,
    pub reserved: u32,
}

#[repr(C)]
pub struct VortexFileInfoC {
    pub row_count: i64,
    pub field_names_csv: *mut c_char,
}

static LAST_ERROR: OnceLock<Mutex<CString>> = OnceLock::new();

enum OutputTarget {
    Local(PathBuf),
    ObjectStore {
        url: Url,
        path: ObjectStorePath,
        store: Arc<dyn ObjectStore>,
    },
}

pub struct VortexWriterHandle {
    schema: SchemaRef,
    sender: Option<Sender<RecordBatch>>,
    worker: Option<JoinHandle<Result<(), String>>>,
    last_error: CString,
    finished: bool,
}

impl VortexWriterHandle {
    fn new(
        schema: SchemaRef,
        sender: Sender<RecordBatch>,
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

fn batch_from_raw(array: *mut FFI_ArrowArray, schema: &SchemaRef) -> Result<RecordBatch, String> {
    if array.is_null() {
        return Err("array must not be null".to_string());
    }
    let ffi_array = unsafe { ptr::read(array) };
    let data = unsafe { from_ffi_and_data_type(ffi_array, DataType::Struct(schema.fields.clone())) }
        .map_err(|err| err.to_string())?;
    let struct_array = StructArray::from(data);
    RecordBatch::try_new(schema.clone(), struct_array.columns().to_vec()).map_err(|err| err.to_string())
}

fn build_dtype(schema: &SchemaRef) -> DType {
    DType::from_arrow(schema.clone())
}

fn parse_writer_options(options: *const VortexWriterOptionsC) -> Result<(), String> {
    if options.is_null() {
        return Ok(());
    }
    let options = unsafe { &*options };
    if options.abi_version > 1 {
        return Err(format!("unsupported Vortex bridge ABI version: {}", options.abi_version));
    }
    Ok(())
}

fn make_object_store(url: &Url) -> Result<Arc<dyn ObjectStore>, String> {
    let (scheme, _) = ObjectStoreScheme::parse(url).map_err(|err| err.to_string())?;
    match scheme {
        ObjectStoreScheme::Local => Ok(Arc::new(LocalFileSystem::default())),
        ObjectStoreScheme::AmazonS3 => {
            let mut builder = AmazonS3Builder::from_env().with_url(url.to_string());
            if let Some(bucket) = url.domain() {
                builder = builder.with_bucket_name(bucket);
            }
            builder
                .build()
                .map(|store| Arc::new(store) as Arc<dyn ObjectStore>)
                .map_err(|err| err.to_string())
        }
        ObjectStoreScheme::MicrosoftAzure => MicrosoftAzureBuilder::new()
            .with_url(url.to_string())
            .build()
            .map(|store| Arc::new(store) as Arc<dyn ObjectStore>)
            .map_err(|err| err.to_string()),
        ObjectStoreScheme::GoogleCloudStorage => GoogleCloudStorageBuilder::new()
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
                let store = make_object_store(&url)?;
                Ok(OutputTarget::ObjectStore { url, path, store })
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
        OutputTarget::ObjectStore { path, store, .. } => {
            let runtime = CurrentThreadRuntime::new();
            runtime.block_on(async {
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

fn write_batches(
    target: OutputTarget,
    dtype: DType,
    receiver: Receiver<RecordBatch>,
) -> Result<(), String> {
    let runtime = CurrentThreadRuntime::new();
    let session = session_for_runtime(&runtime);

    runtime.block_on(async move {
        let stream = receiver.map(|batch| ArrayRef::from_arrow(batch, false));
        let array_stream = ArrayStreamAdapter::new(dtype, stream);

        match target {
            OutputTarget::Local(path) => {
                let mut file = async_fs::File::create(&path)
                    .await
                    .map_err(|err| err.to_string())?;
                session
                    .write_options()
                    .write(&mut file, array_stream)
                    .await
                    .map_err(|err| err.to_string())?;
                VortexWrite::shutdown(&mut file)
                    .await
                    .map_err(|err| err.to_string())?;
            }
            OutputTarget::ObjectStore { store, path, .. } => {
                let mut writer = ObjectStoreWrite::new(store, &path)
                    .await
                    .map_err(|err| err.to_string())?;
                session
                    .write_options()
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
            OutputTarget::ObjectStore { url, store, .. } => session
                .open_options()
                .open_object_store(&store, url.path())
                .await
                .map_err(|err| err.to_string())?,
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

        parse_writer_options(options)?;

        let uri = c_string_to_owned(uri, "uri")?;
        let schema = schema_from_raw(schema)?;
        let dtype = build_dtype(&schema);
        let target = resolve_target(uri.as_str())?;
        ensure_create_target(&target)?;

        let (sender, receiver) = channel::<RecordBatch>(8);
        let worker_dtype = dtype.clone();
        let worker = thread::spawn(move || write_batches(target, worker_dtype, receiver));

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

    let batch = match batch_from_raw(array, &handle.schema) {
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
