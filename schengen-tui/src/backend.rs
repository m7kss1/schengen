use std::io::BufRead;
use std::io::BufReader;
use std::path::Path;
use std::path::PathBuf;
use std::process::Command;
use std::process::Stdio;
use std::sync::mpsc::Sender;
use std::thread;

use anyhow::Context;
use anyhow::Result;
use anyhow::bail;
use serde::Deserialize;

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum RunFormat {
    Parquet,
    Vortex,
}

impl RunFormat {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Parquet => "parquet",
            Self::Vortex => "vortex",
        }
    }

    pub fn title(self) -> &'static str {
        match self {
            Self::Parquet => "Parquet",
            Self::Vortex => "Vortex",
        }
    }

    pub fn ordered() -> [Self; 2] {
        [Self::Parquet, Self::Vortex]
    }

    pub fn from_name(value: &str) -> Option<Self> {
        match value.trim() {
            "parquet" => Some(Self::Parquet),
            "vortex" => Some(Self::Vortex),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(tag = "event")]
pub enum ProgressEvent {
    #[serde(rename = "run_started")]
    RunStarted {
        selected_formats: Vec<String>,
        scale_factor: f64,
        output_path: String,
    },
    #[serde(rename = "format_started")]
    FormatStarted {
        format: String,
        index: i32,
        total_formats: i32,
    },
    #[serde(rename = "table_started")]
    TableStarted {
        format: String,
        table: String,
        total_rows: u64,
        part_count: i32,
    },
    #[serde(rename = "table_progress")]
    TableProgress {
        format: String,
        table: String,
        rows_written: u64,
        total_rows: u64,
        completed_parts: i32,
    },
    #[serde(rename = "table_finished")]
    TableFinished {
        format: String,
        table: String,
        rows_written: u64,
        total_rows: u64,
    },
    #[serde(rename = "table_failed")]
    TableFailed {
        format: String,
        table: String,
        error: String,
    },
    #[serde(rename = "format_finished")]
    FormatFinished {
        format: String,
        success: bool,
    },
    #[serde(rename = "run_finished")]
    RunFinished {
        success: bool,
    },
}

#[derive(Debug)]
pub enum WorkerMessage {
    Progress(ProgressEvent),
    Finished(ProcessOutcome),
}

#[derive(Debug)]
pub struct ProcessOutcome {
    pub format: RunFormat,
    pub success: bool,
    pub stderr: String,
    pub parse_error: Option<String>,
    pub exit_code: Option<i32>,
}

pub fn resolve_backend_path(explicit: Option<PathBuf>, cwd: &Path) -> Result<PathBuf> {
    if let Some(path) = explicit {
        if path.is_file() {
            return Ok(path);
        }
        bail!("backend not found: {}", path.display());
    }

    let candidates = [
        cwd.join("build-vortex").join("schengen_main"),
        cwd.join("build").join("schengen_main"),
        cwd.join("build-lance").join("schengen_main"),
    ];

    for candidate in candidates {
        if candidate.is_file() {
            return Ok(candidate);
        }
    }

    bail!("unable to resolve schengen_main in build-vortex, build, or build-lance");
}

pub fn probe_backend_formats(path: &Path) -> Result<Vec<RunFormat>> {
    let output = Command::new(path)
        .arg("--list-formats")
        .output()
        .with_context(|| format!("failed to run {} --list-formats", path.display()))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        bail!(
            "backend format probe failed (exit {:?}): {}",
            output.status.code(),
            stderr.trim()
        );
    }

    let stdout = String::from_utf8(output.stdout).context("backend --list-formats returned non-utf8 stdout")?;
    Ok(probe_backend_formats_from_output(&stdout))
}

pub fn probe_backend_formats_from_output(stdout: &str) -> Vec<RunFormat> {
    let mut out = Vec::new();
    for line in stdout.lines() {
        if let Some(format) = RunFormat::from_name(line) {
            if !out.contains(&format) {
                out.push(format);
            }
        }
    }
    out
}

pub fn spawn_backend_worker(
    backend_path: PathBuf,
    format: RunFormat,
    args: Vec<String>,
    tx: Sender<WorkerMessage>,
) -> thread::JoinHandle<()> {
    thread::spawn(move || {
        let mut command = Command::new(&backend_path);
        command.args(&args);
        command.stdout(Stdio::piped());
        command.stderr(Stdio::piped());

        let mut child = match command.spawn() {
            Ok(child) => child,
            Err(error) => {
                let _ = tx.send(WorkerMessage::Finished(ProcessOutcome {
                    format,
                    success: false,
                    stderr: format!("failed to spawn backend: {error}"),
                    parse_error: None,
                    exit_code: None,
                }));
                return;
            }
        };

        let stderr_handle = child.stderr.take().map(|stderr| {
            thread::spawn(move || {
                let mut stderr_buffer = String::new();
                let mut reader = BufReader::new(stderr);
                let _ = std::io::Read::read_to_string(&mut reader, &mut stderr_buffer);
                stderr_buffer
            })
        });

        let mut parse_error: Option<String> = None;
        if let Some(stdout) = child.stdout.take() {
            let reader = BufReader::new(stdout);
            for line in reader.lines() {
                match line {
                    Ok(line) => {
                        if parse_error.is_none() {
                            match serde_json::from_str::<ProgressEvent>(&line) {
                                Ok(event) => {
                                    let _ = tx.send(WorkerMessage::Progress(event));
                                }
                                Err(error) => {
                                    parse_error = Some(format!("failed to parse backend progress line: {error}: {line}"));
                                }
                            }
                        }
                    }
                    Err(error) => {
                        if parse_error.is_none() {
                            parse_error = Some(format!("failed to read backend stdout: {error}"));
                        }
                        break;
                    }
                }
            }
        }

        let status = child.wait().ok();
        let stderr = stderr_handle
            .and_then(|handle| handle.join().ok())
            .unwrap_or_default();
        let exit_code = status.and_then(|status| status.code());
        let success = status.map(|status| status.success()).unwrap_or(false) && parse_error.is_none();

        let _ = tx.send(WorkerMessage::Finished(ProcessOutcome {
            format,
            success,
            stderr,
            parse_error,
            exit_code,
        }));
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    fn unique_temp_dir(name: &str) -> PathBuf {
        let mut path = std::env::temp_dir();
        let nanos = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .expect("time went backwards")
            .as_nanos();
        path.push(format!("schengen-tui-{name}-{nanos}"));
        path
    }

    #[test]
    fn resolve_backend_path_prefers_build_vortex() {
        let root = unique_temp_dir("resolve");
        fs::create_dir_all(root.join("build-vortex")).expect("create build-vortex");
        fs::create_dir_all(root.join("build-lance")).expect("create build-lance");
        fs::write(root.join("build-vortex").join("schengen_main"), "").expect("write build-vortex binary");
        fs::write(root.join("build-lance").join("schengen_main"), "").expect("write build-lance binary");

        let resolved = resolve_backend_path(None, &root).expect("resolve backend");
        assert_eq!(resolved, root.join("build-vortex").join("schengen_main"));

        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn probe_backend_formats_from_output_filters_supported_formats() {
        let parsed = probe_backend_formats_from_output("parquet\nignored\nvortex\nparquet\n");
        assert_eq!(parsed, vec![RunFormat::Parquet, RunFormat::Vortex]);
    }
}
