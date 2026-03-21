use std::ffi::{CStr, CString, c_char};
use std::ptr;
use std::sync::mpsc::{Receiver, SyncSender, sync_channel};
use std::sync::{Arc, Mutex, OnceLock};
use std::thread::{self, JoinHandle};

use arrow_array::RecordBatch;
use arrow_array::RecordBatchReader;
use arrow_array::StructArray;
use arrow_array::ffi::{FFI_ArrowArray, from_ffi_and_data_type};
use arrow_schema::ffi::FFI_ArrowSchema;
use arrow_schema::{ArrowError, DataType, Schema as ArrowSchema, SchemaRef};
use lance::dataset::{WriteMode, WriteParams};
use lance::{Dataset, Error as LanceError};

#[repr(C)]
pub struct LanceWriterOptionsC {
    pub max_rows_per_file: i64,
    pub max_rows_per_group: i64,
    pub max_bytes_per_file: i64,
}

#[repr(C)]
pub struct LanceDatasetInfoC {
    pub row_count: i64,
    pub field_names_csv: *mut c_char,
}

static LAST_ERROR: OnceLock<Mutex<CString>> = OnceLock::new();

fn global_error_slot() -> &'static Mutex<CString> {
    LAST_ERROR.get_or_init(|| Mutex::new(CString::new("").unwrap()))
}

fn sanitize_message(message: impl Into<String>) -> CString {
    let message = message.into().replace('\0', " ");
    CString::new(message).unwrap_or_else(|_| CString::new("unknown lance bridge error").unwrap())
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

fn runtime() -> Result<tokio::runtime::Runtime, String> {
    tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .map_err(|err| err.to_string())
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

fn build_write_params(options: &LanceWriterOptionsC) -> Result<WriteParams, String> {
    let mut params = WriteParams::default();
    params.mode = WriteMode::Create;
    if options.max_rows_per_file < 0 {
        return Err("max_rows_per_file must be >= 0".to_string());
    }
    if options.max_rows_per_group < 0 {
        return Err("max_rows_per_group must be >= 0".to_string());
    }
    if options.max_bytes_per_file < 0 {
        return Err("max_bytes_per_file must be >= 0".to_string());
    }
    if options.max_rows_per_file > 0 {
        params.max_rows_per_file = options.max_rows_per_file as usize;
    }
    if options.max_rows_per_group > 0 {
        params.max_rows_per_group = options.max_rows_per_group as usize;
    }
    if options.max_bytes_per_file > 0 {
        params.max_bytes_per_file = options.max_bytes_per_file as usize;
    }
    Ok(params)
}

fn validate_create_target(uri: &str) -> Result<(), String> {
    let runtime = runtime()?;
    match runtime.block_on(Dataset::open(uri)) {
        Ok(_) => Err(format!("dataset already exists: {uri}")),
        Err(LanceError::DatasetNotFound { .. } | LanceError::NotFound { .. }) => Ok(()),
        Err(err) => Err(err.to_string()),
    }
}

struct ChannelBatchReader {
    schema: SchemaRef,
    receiver: Receiver<Option<RecordBatch>>,
}

impl Iterator for ChannelBatchReader {
    type Item = Result<RecordBatch, ArrowError>;

    fn next(&mut self) -> Option<Self::Item> {
        match self.receiver.recv() {
            Ok(Some(batch)) => Some(Ok(batch)),
            Ok(None) | Err(_) => None,
        }
    }
}

impl RecordBatchReader for ChannelBatchReader {
    fn schema(&self) -> SchemaRef {
        self.schema.clone()
    }
}

pub struct LanceWriterHandle {
    schema: SchemaRef,
    sender: Option<SyncSender<Option<RecordBatch>>>,
    worker: Option<JoinHandle<Result<(), String>>>,
    last_error: CString,
    finished: bool,
}

impl LanceWriterHandle {
    fn new(
        schema: SchemaRef,
        sender: SyncSender<Option<RecordBatch>>,
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

#[unsafe(no_mangle)]
pub extern "C" fn lance_writer_open(
    uri: *const c_char,
    schema: *mut FFI_ArrowSchema,
    options: *const LanceWriterOptionsC,
    out_handle: *mut *mut LanceWriterHandle,
) -> i32 {
    clear_global_error();

    let result = (|| -> Result<*mut LanceWriterHandle, String> {
        if out_handle.is_null() {
            return Err("out_handle must not be null".to_string());
        }

        let uri = c_string_to_owned(uri, "uri")?;
        let schema = schema_from_raw(schema)?;
        let write_params = if options.is_null() {
            WriteParams::default()
        } else {
            build_write_params(unsafe { &*options })?
        };
        validate_create_target(uri.as_str())?;

        let (sender, receiver) = sync_channel::<Option<RecordBatch>>(8);
        let reader = ChannelBatchReader {
            schema: schema.clone(),
            receiver,
        };

        let worker = thread::spawn(move || -> Result<(), String> {
            let runtime = runtime()?;
            runtime
                .block_on(Dataset::write(reader, uri.as_str(), Some(write_params)))
                .map(|_| ())
                .map_err(|err| err.to_string())
        });

        Ok(Box::into_raw(Box::new(LanceWriterHandle::new(schema, sender, worker))))
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
pub extern "C" fn lance_writer_begin_partition(
    handle: *mut LanceWriterHandle,
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
pub extern "C" fn lance_writer_push_batch(
    handle: *mut LanceWriterHandle,
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

    match handle.sender.as_ref() {
        Some(sender) => sender
            .send(Some(batch))
            .map(|_| 0)
            .unwrap_or_else(|err| handle.set_error(err.to_string())),
        None => handle.set_error("writer channel is closed"),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn lance_writer_end_partition(handle: *mut LanceWriterHandle) -> i32 {
    if handle.is_null() {
        return set_global_error("handle must not be null");
    }
    let handle = unsafe { &mut *handle };
    handle.clear_error();
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn lance_writer_finish(handle: *mut LanceWriterHandle) -> i32 {
    if handle.is_null() {
        return set_global_error("handle must not be null");
    }

    let handle = unsafe { &mut *handle };
    handle.clear_error();

    if handle.finished {
        return 0;
    }

    if let Some(sender) = handle.sender.take() {
        sender
            .send(None)
            .map_err(|err| err.to_string())
            .unwrap_or_else(|err| {
                let _ = handle.set_error(err);
            });
    }

    if let Some(worker) = handle.worker.take() {
        match worker.join() {
            Ok(Ok(())) => {
                handle.finished = true;
                0
            }
            Ok(Err(err)) => handle.set_error(err),
            Err(_) => handle.set_error("lance writer thread panicked"),
        }
    } else {
        handle.finished = true;
        0
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn lance_writer_destroy(handle: *mut LanceWriterHandle) {
    if handle.is_null() {
        return;
    }

    let mut handle = unsafe { Box::from_raw(handle) };
    if !handle.finished {
        let _ = lance_writer_finish(&mut *handle);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn lance_last_error(handle: *const LanceWriterHandle) -> *const c_char {
    if handle.is_null() {
        return lance_bridge_last_error();
    }
    let handle = unsafe { &*handle };
    handle.last_error.as_ptr()
}

#[unsafe(no_mangle)]
pub extern "C" fn lance_bridge_last_error() -> *const c_char {
    match global_error_slot().lock() {
        Ok(guard) => guard.as_ptr(),
        Err(_) => c"failed to acquire bridge error lock".as_ptr(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn lance_dataset_inspect(uri: *const c_char, out_info: *mut LanceDatasetInfoC) -> i32 {
    clear_global_error();

    let result = (|| -> Result<(), String> {
        if out_info.is_null() {
            return Err("out_info must not be null".to_string());
        }

        let uri = c_string_to_owned(uri, "uri")?;
        let runtime = runtime()?;
        let dataset = runtime
            .block_on(Dataset::open(uri.as_str()))
            .map_err(|err| err.to_string())?;
        let row_count = runtime
            .block_on(dataset.count_rows(None))
            .map_err(|err| err.to_string())? as i64;

        let field_names_csv = dataset
            .schema()
            .fields
            .iter()
            .map(|field| field.name.as_str())
            .collect::<Vec<_>>()
            .join(",");

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
pub extern "C" fn lance_dataset_info_destroy(info: *mut LanceDatasetInfoC) {
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
