use std::collections::HashMap;
use std::path::PathBuf;
use std::sync::mpsc;
use std::sync::mpsc::Receiver;
use std::thread::JoinHandle;
use std::time::Duration;

use anyhow::Result;
use crossterm::event::KeyCode;
use crossterm::event::KeyEvent;
use ratatui::layout::Alignment;
use ratatui::layout::Constraint;
use ratatui::layout::Direction;
use ratatui::layout::Layout;
use ratatui::prelude::Color;
use ratatui::prelude::Frame;
use ratatui::prelude::Line;
use ratatui::prelude::Modifier;
use ratatui::prelude::Style;
use ratatui::widgets::Block;
use ratatui::widgets::Borders;
use ratatui::widgets::Cell;
use ratatui::widgets::Paragraph;
use ratatui::widgets::Row;
use ratatui::widgets::Table;
use ratatui::widgets::Tabs;
use ratatui::widgets::Wrap;

use crate::backend::probe_backend_formats;
use crate::backend::resolve_backend_path;
use crate::backend::spawn_backend_worker;
use crate::backend::ProcessOutcome;
use crate::backend::ProgressEvent;
use crate::backend::RunFormat;
use crate::backend::WorkerMessage;

const TABLE_ORDER: &[&str] = &[
    "region",
    "nation",
    "supplier",
    "part",
    "partsupp",
    "customer",
    "orders",
    "lineitem",
];

const SPINNER_FRAMES: &[&str] = &["|", "/", "-", "\\"];

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SidebarTab {
    Formats,
    Scale,
}

impl SidebarTab {
    fn index(self) -> usize {
        match self {
            Self::Formats => 0,
            Self::Scale => 1,
        }
    }

    fn next(self) -> Self {
        match self {
            Self::Formats => Self::Scale,
            Self::Scale => Self::Formats,
        }
    }

    fn previous(self) -> Self {
        self.next()
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum CompressionChoice {
    Snappy,
    Uncompressed,
}

impl CompressionChoice {
    fn as_cli_value(self) -> &'static str {
        match self {
            Self::Snappy => "snappy",
            Self::Uncompressed => "uncompressed",
        }
    }

    fn cycle(self, direction: i32) -> Self {
        match (self, direction.signum()) {
            (Self::Snappy, 1) => Self::Uncompressed,
            (Self::Snappy, -1) => Self::Uncompressed,
            (Self::Uncompressed, 1) => Self::Snappy,
            (Self::Uncompressed, -1) => Self::Snappy,
            _ => self,
        }
    }
}

#[derive(Clone, Debug)]
struct ParquetConfig {
    compression: CompressionChoice,
    use_threads: bool,
    row_group_bytes: String,
    max_row_group_rows: String,
    output_buffer_bytes: String,
}

impl Default for ParquetConfig {
    fn default() -> Self {
        Self {
            compression: CompressionChoice::Snappy,
            use_threads: false,
            row_group_bytes: (7 * 1024 * 1024).to_string(),
            max_row_group_rows: "0".to_string(),
            output_buffer_bytes: (21 * 1024 * 1024).to_string(),
        }
    }
}

#[derive(Clone, Debug)]
struct VortexConfig {
    target_partition_rows: String,
    row_block_size: String,
    output_buffer_bytes: String,
}

impl Default for VortexConfig {
    fn default() -> Self {
        Self {
            target_partition_rows: "0".to_string(),
            row_block_size: "8192".to_string(),
            output_buffer_bytes: (16 * 1024 * 1024).to_string(),
        }
    }
}

#[derive(Clone, Debug)]
struct FormatToggle<T> {
    supported: bool,
    enabled: bool,
    expanded: bool,
    settings: T,
}

#[derive(Clone, Debug)]
struct AppConfig {
    scale_factor: String,
    output_path: String,
    parquet: FormatToggle<ParquetConfig>,
    vortex: FormatToggle<VortexConfig>,
}

impl AppConfig {
    fn from_supported_formats(supported_formats: &[RunFormat]) -> Self {
        let parquet_supported = supported_formats.contains(&RunFormat::Parquet);
        let vortex_supported = supported_formats.contains(&RunFormat::Vortex);

        Self {
            scale_factor: "1.0".to_string(),
            output_path: "./out".to_string(),
            parquet: FormatToggle {
                supported: parquet_supported,
                enabled: parquet_supported,
                expanded: true,
                settings: ParquetConfig::default(),
            },
            vortex: FormatToggle {
                supported: vortex_supported,
                enabled: vortex_supported,
                expanded: false,
                settings: VortexConfig::default(),
            },
        }
    }

    fn snapshot(&self) -> ConfigSnapshot {
        ConfigSnapshot {
            scale_factor: self.scale_factor.clone(),
            output_path: self.output_path.clone(),
            parquet: if self.parquet.supported && self.parquet.enabled {
                Some(self.parquet.settings.clone())
            } else {
                None
            },
            vortex: if self.vortex.supported && self.vortex.enabled {
                Some(self.vortex.settings.clone())
            } else {
                None
            },
        }
    }
}

#[derive(Clone, Debug)]
pub struct ConfigSnapshot {
    scale_factor: String,
    output_path: String,
    parquet: Option<ParquetConfig>,
    vortex: Option<VortexConfig>,
}

impl ConfigSnapshot {
    fn selected_formats(&self) -> Vec<RunFormat> {
        let mut formats = Vec::new();
        for format in RunFormat::ordered() {
            match format {
                RunFormat::Parquet if self.parquet.is_some() => formats.push(format),
                RunFormat::Vortex if self.vortex.is_some() => formats.push(format),
                _ => {}
            }
        }
        formats
    }

    fn command_args_for(&self, format: RunFormat) -> Vec<String> {
        let mut args = vec![
            "--progress-json".to_string(),
            "--scale-factor".to_string(),
            self.scale_factor.clone(),
            "--output-format".to_string(),
            format.as_str().to_string(),
            "--output-path".to_string(),
            self.output_path.clone(),
        ];

        match format {
            RunFormat::Parquet => {
                if let Some(parquet) = &self.parquet {
                    if parquet.use_threads {
                        args.push("--parquet-use-threads".to_string());
                    }
                    args.extend([
                        "--parquet-row-group-bytes".to_string(),
                        parquet.row_group_bytes.clone(),
                        "--parquet-max-row-group-rows".to_string(),
                        parquet.max_row_group_rows.clone(),
                        "--parquet-output-buffer-bytes".to_string(),
                        parquet.output_buffer_bytes.clone(),
                        "--parquet-compression".to_string(),
                        parquet.compression.as_cli_value().to_string(),
                    ]);
                }
            }
            RunFormat::Vortex => {
                if let Some(vortex) = &self.vortex {
                    args.extend([
                        "--vortex-target-partition-rows".to_string(),
                        vortex.target_partition_rows.clone(),
                        "--vortex-row-block-size".to_string(),
                        vortex.row_block_size.clone(),
                        "--vortex-output-buffer-bytes".to_string(),
                        vortex.output_buffer_bytes.clone(),
                    ]);
                }
            }
        }

        args
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum FieldId {
    ParquetEnabled,
    ParquetExpanded,
    ParquetCompression,
    ParquetUseThreads,
    ParquetRowGroupBytes,
    ParquetMaxRowGroupRows,
    ParquetOutputBufferBytes,
    VortexEnabled,
    VortexExpanded,
    VortexTargetPartitionRows,
    VortexRowBlockSize,
    VortexOutputBufferBytes,
    ScaleFactor,
    OutputPath,
    Start,
}

#[derive(Clone, Debug)]
struct EditState {
    field: FieldId,
    buffer: String,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum TableStatusKind {
    Pending,
    Running,
    Done,
    Error,
}

#[derive(Clone, Debug)]
struct TableStatus {
    kind: TableStatusKind,
    rows_written: u64,
    total_rows: u64,
    completed_parts: i32,
    detail: Option<String>,
}

impl Default for TableStatus {
    fn default() -> Self {
        Self {
            kind: TableStatusKind::Pending,
            rows_written: 0,
            total_rows: 0,
            completed_parts: 0,
            detail: None,
        }
    }
}

struct RunState {
    snapshot: ConfigSnapshot,
    queue: Vec<RunFormat>,
    current_index: usize,
    tables: HashMap<String, TableStatus>,
    rx: Receiver<WorkerMessage>,
    worker: Option<JoinHandle<()>>,
}

impl RunState {
    fn current_format(&self) -> RunFormat {
        self.queue[self.current_index]
    }
}

pub struct App {
    tab: SidebarTab,
    focus: FieldId,
    editing: Option<EditState>,
    config: AppConfig,
    backend_path: Option<PathBuf>,
    backend_error: Option<String>,
    supported_formats: Vec<RunFormat>,
    run: Option<RunState>,
    spinner_index: usize,
    should_quit: bool,
    quit_after_run: bool,
    status_message: Option<String>,
    last_completed_formats: Vec<RunFormat>,
    last_run_snapshot: Option<ConfigSnapshot>,
    last_error: Option<String>,
}

pub struct TableRowView {
    pub table: String,
    pub status: String,
    pub rows: String,
    pub percent: String,
}

impl App {
    pub fn new(backend_override: Option<PathBuf>) -> Self {
        let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
        let mut backend_path = None;
        let mut backend_error = None;
        let mut supported_formats = Vec::new();

        match resolve_backend_path(backend_override, &cwd).and_then(|path| {
            let formats = probe_backend_formats(&path)?;
            Ok((path, formats))
        }) {
            Ok((path, formats)) => {
                backend_path = Some(path);
                supported_formats = formats;
            }
            Err(error) => {
                backend_error = Some(error.to_string());
            }
        }

        let config = AppConfig::from_supported_formats(&supported_formats);

        Self {
            tab: SidebarTab::Formats,
            focus: FieldId::ParquetEnabled,
            editing: None,
            config,
            backend_path,
            backend_error,
            supported_formats,
            run: None,
            spinner_index: 0,
            should_quit: false,
            quit_after_run: false,
            status_message: None,
            last_completed_formats: Vec::new(),
            last_run_snapshot: None,
            last_error: None,
        }
    }

    pub fn should_quit(&self) -> bool {
        self.should_quit
    }

    pub fn tick(&mut self) {
        self.spinner_index = (self.spinner_index + 1) % SPINNER_FRAMES.len();
    }

    pub fn handle_key(&mut self, key: KeyEvent) {
        if let Some(edit_state) = &mut self.editing {
            match key.code {
                KeyCode::Esc => {
                    self.editing = None;
                }
                KeyCode::Enter => {
                    let field = edit_state.field;
                    let buffer = edit_state.buffer.clone();
                    self.apply_edit(field, buffer);
                    self.editing = None;
                }
                KeyCode::Backspace => {
                    edit_state.buffer.pop();
                }
                KeyCode::Char(ch) => {
                    edit_state.buffer.push(ch);
                }
                _ => {}
            }
            return;
        }

        if self.run.is_some() {
            if let KeyCode::Char('q') = key.code {
                self.quit_after_run = true;
                self.status_message = Some("Will exit after the current generation queue finishes".to_string());
            }
            return;
        }

        match key.code {
            KeyCode::Char('q') => {
                self.should_quit = true;
            }
            KeyCode::Tab => {
                self.tab = self.tab.next();
                self.ensure_focus_visible();
            }
            KeyCode::BackTab => {
                self.tab = self.tab.previous();
                self.ensure_focus_visible();
            }
            KeyCode::Up => self.move_focus(-1),
            KeyCode::Down => self.move_focus(1),
            KeyCode::Left => self.adjust_focused(-1),
            KeyCode::Right => self.adjust_focused(1),
            KeyCode::Enter => self.activate_focused_field(),
            _ => {}
        }
    }

    pub fn pump_messages(&mut self) {
        loop {
            let message = match self.run.as_ref() {
                Some(run) => run.rx.try_recv().ok(),
                None => None,
            };

            let Some(message) = message else {
                break;
            };

            match message {
                WorkerMessage::Progress(event) => self.apply_progress_event(event),
                WorkerMessage::Finished(outcome) => self.finish_current_worker(outcome),
            }
        }
    }

    pub fn draw(&self, frame: &mut Frame<'_>) {
        let [sidebar, main] = Layout::default()
            .direction(Direction::Horizontal)
            .constraints([Constraint::Length(44), Constraint::Min(0)])
            .areas(frame.area());

        let [tabs_area, settings_area, output_area] = Layout::default()
            .direction(Direction::Vertical)
            .constraints([Constraint::Length(3), Constraint::Min(10), Constraint::Length(8)])
            .areas(sidebar);

        let tabs = Tabs::new(["Formats", "Scale"])
            .block(Block::default().borders(Borders::ALL).title("Sidebar"))
            .highlight_style(Style::default().fg(Color::Black).bg(Color::White).add_modifier(Modifier::BOLD))
            .select(Some(self.tab.index()));
        frame.render_widget(tabs, tabs_area);

        let settings = Paragraph::new(self.sidebar_lines())
            .block(Block::default().borders(Borders::ALL).title("Settings"))
            .wrap(Wrap { trim: false });
        frame.render_widget(settings, settings_area);

        let output = Paragraph::new(self.output_lines())
            .block(Block::default().borders(Borders::ALL).title("Output"))
            .wrap(Wrap { trim: false });
        frame.render_widget(output, output_area);

        if self.run.is_some() {
            let [banner_area, table_area, footer_area] = Layout::default()
                .direction(Direction::Vertical)
                .constraints([Constraint::Length(3), Constraint::Min(8), Constraint::Length(4)])
                .areas(main);

            let banner = Paragraph::new(self.running_banner())
                .block(Block::default().borders(Borders::ALL).title("Status"))
                .alignment(Alignment::Center);
            frame.render_widget(banner, banner_area);

            let header = Row::new(vec![
                Cell::from("Table"),
                Cell::from("Status"),
                Cell::from("Rows / Total"),
                Cell::from("%"),
            ])
            .style(Style::default().add_modifier(Modifier::BOLD));
            let rows = self
                .table_rows()
                .into_iter()
                .map(|row| Row::new(vec![row.table, row.status, row.rows, row.percent]));
            let table = Table::new(
                rows,
                [
                    Constraint::Length(12),
                    Constraint::Length(12),
                    Constraint::Length(24),
                    Constraint::Length(8),
                ],
            )
            .header(header)
            .block(Block::default().borders(Borders::ALL).title("Tables"))
            .column_spacing(1);
            frame.render_widget(table, table_area);

            let footer = Paragraph::new(self.footer_lines())
                .block(Block::default().borders(Borders::ALL).title("Notes"))
                .wrap(Wrap { trim: false });
            frame.render_widget(footer, footer_area);
        } else {
            let body = Paragraph::new(self.idle_lines())
                .block(Block::default().borders(Borders::ALL).title("Schengen TUI"))
                .wrap(Wrap { trim: false });
            frame.render_widget(body, main);
        }
    }

    fn sidebar_lines(&self) -> Vec<Line<'static>> {
        let mut lines = Vec::new();
        match self.tab {
            SidebarTab::Formats => {
                lines.push(self.field_line(FieldId::ParquetEnabled, format!("Parquet [{}]", self.toggle_mark(self.config.parquet.enabled, self.config.parquet.supported))));
                lines.push(self.field_line(FieldId::ParquetExpanded, format!("Parquet settings [{}]", if self.config.parquet.expanded { "-" } else { "+" })));
                if self.config.parquet.expanded {
                    lines.push(self.field_line(
                        FieldId::ParquetCompression,
                        format!("Compression: {}", self.config.parquet.settings.compression.as_cli_value()),
                    ));
                    lines.push(self.field_line(
                        FieldId::ParquetUseThreads,
                        format!("Use threads [{}]", self.toggle_mark(self.config.parquet.settings.use_threads, true)),
                    ));
                    lines.push(self.field_line(
                        FieldId::ParquetRowGroupBytes,
                        format!("row_group_bytes: {}", self.value_for(FieldId::ParquetRowGroupBytes)),
                    ));
                    lines.push(self.field_line(
                        FieldId::ParquetMaxRowGroupRows,
                        format!("max_row_group_rows: {}", self.value_for(FieldId::ParquetMaxRowGroupRows)),
                    ));
                    lines.push(self.field_line(
                        FieldId::ParquetOutputBufferBytes,
                        format!("output_buffer_bytes: {}", self.value_for(FieldId::ParquetOutputBufferBytes)),
                    ));
                }

                lines.push(Line::from(""));
                lines.push(self.field_line(FieldId::VortexEnabled, format!("Vortex [{}]", self.toggle_mark(self.config.vortex.enabled, self.config.vortex.supported))));
                lines.push(self.field_line(FieldId::VortexExpanded, format!("Vortex settings [{}]", if self.config.vortex.expanded { "-" } else { "+" })));
                if self.config.vortex.expanded {
                    lines.push(self.field_line(
                        FieldId::VortexTargetPartitionRows,
                        format!("target_partition_rows: {}", self.value_for(FieldId::VortexTargetPartitionRows)),
                    ));
                    lines.push(self.field_line(
                        FieldId::VortexRowBlockSize,
                        format!("row_block_size: {}", self.value_for(FieldId::VortexRowBlockSize)),
                    ));
                    lines.push(self.field_line(
                        FieldId::VortexOutputBufferBytes,
                        format!("output_buffer_bytes: {}", self.value_for(FieldId::VortexOutputBufferBytes)),
                    ));
                }
            }
            SidebarTab::Scale => {
                lines.push(self.field_line(
                    FieldId::ScaleFactor,
                    format!("Scale factor: {}", self.value_for(FieldId::ScaleFactor)),
                ));
            }
        }
        lines
    }

    fn output_lines(&self) -> Vec<Line<'static>> {
        let backend_line = match (&self.backend_path, &self.backend_error) {
            (Some(path), _) => format!("Backend: {}", path.display()),
            (_, Some(error)) => format!("Backend error: {error}"),
            _ => "Backend: <unresolved>".to_string(),
        };

        let mut lines = vec![
            self.field_line(FieldId::OutputPath, format!("Output path: {}", self.value_for(FieldId::OutputPath))),
            Line::from(backend_line),
            Line::from(format!(
                "Formats: {}",
                self.format_summary(&self.config.snapshot().selected_formats())
            )),
            self.field_line(
                FieldId::Start,
                if let Some(reason) = self.can_start_reason() {
                    format!("[ Start ] disabled: {reason}")
                } else {
                    "[ Start ] ready".to_string()
                },
            ),
        ];

        if let Some(message) = &self.status_message {
            lines.push(Line::from(message.clone()));
        }

        lines
    }

    fn idle_lines(&self) -> Vec<Line<'static>> {
        let selected_formats = self.config.snapshot().selected_formats();
        let mut lines = vec![
            Line::from("Idle"),
            Line::from(format!("Selected formats: {}", self.format_summary(&selected_formats))),
            Line::from(format!("Scale factor: {}", self.config.scale_factor)),
            Line::from(format!("Output path: {}", self.config.output_path)),
            Line::from("Hotkeys: Tab/Shift-Tab switch tabs"),
            Line::from("Hotkeys: Up/Down move focus, Left/Right change toggles"),
            Line::from("Hotkeys: Enter edits text fields or starts generation"),
            Line::from("Hotkeys: q quits while idle"),
        ];

        if !self.supported_formats.is_empty() {
            lines.push(Line::from(format!(
                "Backend supports: {}",
                self.format_summary(&self.supported_formats)
            )));
        }
        if !self.last_completed_formats.is_empty() {
            lines.push(Line::from(format!(
                "Last completed formats: {}",
                self.format_summary(&self.last_completed_formats)
            )));
        }
        if let Some(error) = &self.last_error {
            lines.push(Line::from(format!("Last error: {error}")));
        }
        if let Some(editing) = &self.editing {
            lines.push(Line::from(format!("Editing {:?}: {}", editing.field, editing.buffer)));
        }

        lines
    }

    fn running_banner(&self) -> Line<'static> {
        let run = self.run.as_ref().expect("running banner without run state");
        let format = run.current_format();
        let spinner = SPINNER_FRAMES[self.spinner_index];
        Line::from(format!(
            "{spinner} Running {} ({}/{})",
            format.title(),
            run.current_index + 1,
            run.queue.len()
        ))
    }

    fn footer_lines(&self) -> Vec<Line<'static>> {
        let mut lines = vec![Line::from("Settings are locked while generation is running.")];
        if let Some(message) = &self.status_message {
            lines.push(Line::from(message.clone()));
        }
        if let Some(error) = &self.last_error {
            lines.push(Line::from(format!("Last error: {error}")));
        }
        lines
    }

    fn table_rows(&self) -> Vec<TableRowView> {
        let mut rows = Vec::new();
        let Some(run) = &self.run else {
            return rows;
        };

        for table_name in TABLE_ORDER {
            let status = run.tables.get(*table_name).cloned().unwrap_or_default();
            let status_text = match status.kind {
                TableStatusKind::Pending => "pending".to_string(),
                TableStatusKind::Running => format!("running ({})", status.completed_parts),
                TableStatusKind::Done => "done".to_string(),
                TableStatusKind::Error => "error".to_string(),
            };
            let rows_text = if status.total_rows > 0 {
                format!("{}/{}", status.rows_written, status.total_rows)
            } else {
                "-".to_string()
            };
            let percent_text = if status.total_rows > 0 {
                let percent = ((status.rows_written as f64 / status.total_rows as f64) * 100.0).round() as i32;
                format!("{percent}%")
            } else {
                "--".to_string()
            };

            rows.push(TableRowView {
                table: (*table_name).to_string(),
                status: status_text,
                rows: rows_text,
                percent: percent_text,
            });
        }

        rows
    }

    fn move_focus(&mut self, direction: i32) {
        let visible = self.visible_fields();
        if visible.is_empty() {
            return;
        }
        let current_index = visible
            .iter()
            .position(|field| *field == self.focus)
            .unwrap_or(0) as i32;
        let next_index = (current_index + direction).clamp(0, (visible.len() - 1) as i32) as usize;
        self.focus = visible[next_index];
    }

    fn ensure_focus_visible(&mut self) {
        let visible = self.visible_fields();
        if !visible.contains(&self.focus) {
            if let Some(first) = visible.first().copied() {
                self.focus = first;
            }
        }
    }

    fn visible_fields(&self) -> Vec<FieldId> {
        let mut fields = Vec::new();
        match self.tab {
            SidebarTab::Formats => {
                fields.push(FieldId::ParquetEnabled);
                fields.push(FieldId::ParquetExpanded);
                if self.config.parquet.expanded {
                    fields.extend([
                        FieldId::ParquetCompression,
                        FieldId::ParquetUseThreads,
                        FieldId::ParquetRowGroupBytes,
                        FieldId::ParquetMaxRowGroupRows,
                        FieldId::ParquetOutputBufferBytes,
                    ]);
                }
                fields.push(FieldId::VortexEnabled);
                fields.push(FieldId::VortexExpanded);
                if self.config.vortex.expanded {
                    fields.extend([
                        FieldId::VortexTargetPartitionRows,
                        FieldId::VortexRowBlockSize,
                        FieldId::VortexOutputBufferBytes,
                    ]);
                }
            }
            SidebarTab::Scale => {
                fields.push(FieldId::ScaleFactor);
            }
        }
        fields.push(FieldId::OutputPath);
        fields.push(FieldId::Start);
        fields
    }

    fn adjust_focused(&mut self, direction: i32) {
        match self.focus {
            FieldId::ParquetEnabled => {
                if self.config.parquet.supported {
                    self.config.parquet.enabled = !self.config.parquet.enabled;
                }
            }
            FieldId::ParquetExpanded => {
                self.config.parquet.expanded = !self.config.parquet.expanded;
                self.ensure_focus_visible();
            }
            FieldId::ParquetCompression => {
                self.config.parquet.settings.compression =
                    self.config.parquet.settings.compression.cycle(direction);
            }
            FieldId::ParquetUseThreads => {
                self.config.parquet.settings.use_threads = !self.config.parquet.settings.use_threads;
            }
            FieldId::VortexEnabled => {
                if self.config.vortex.supported {
                    self.config.vortex.enabled = !self.config.vortex.enabled;
                }
            }
            FieldId::VortexExpanded => {
                self.config.vortex.expanded = !self.config.vortex.expanded;
                self.ensure_focus_visible();
            }
            _ => {}
        }
    }

    fn activate_focused_field(&mut self) {
        match self.focus {
            FieldId::ParquetEnabled
            | FieldId::ParquetExpanded
            | FieldId::ParquetCompression
            | FieldId::ParquetUseThreads
            | FieldId::VortexEnabled
            | FieldId::VortexExpanded => self.adjust_focused(1),
            FieldId::ParquetRowGroupBytes
            | FieldId::ParquetMaxRowGroupRows
            | FieldId::ParquetOutputBufferBytes
            | FieldId::VortexTargetPartitionRows
            | FieldId::VortexRowBlockSize
            | FieldId::VortexOutputBufferBytes
            | FieldId::ScaleFactor
            | FieldId::OutputPath => {
                self.editing = Some(EditState {
                    field: self.focus,
                    buffer: self.value_for(self.focus),
                });
            }
            FieldId::Start => {
                self.start_run();
            }
        }
    }

    fn apply_edit(&mut self, field: FieldId, value: String) {
        match field {
            FieldId::ParquetRowGroupBytes => self.config.parquet.settings.row_group_bytes = value,
            FieldId::ParquetMaxRowGroupRows => self.config.parquet.settings.max_row_group_rows = value,
            FieldId::ParquetOutputBufferBytes => self.config.parquet.settings.output_buffer_bytes = value,
            FieldId::VortexTargetPartitionRows => self.config.vortex.settings.target_partition_rows = value,
            FieldId::VortexRowBlockSize => self.config.vortex.settings.row_block_size = value,
            FieldId::VortexOutputBufferBytes => self.config.vortex.settings.output_buffer_bytes = value,
            FieldId::ScaleFactor => self.config.scale_factor = value,
            FieldId::OutputPath => self.config.output_path = value,
            _ => {}
        }
    }

    fn value_for(&self, field: FieldId) -> String {
        if let Some(editing) = &self.editing {
            if editing.field == field {
                return format!("{}|", editing.buffer);
            }
        }

        match field {
            FieldId::ParquetRowGroupBytes => self.config.parquet.settings.row_group_bytes.clone(),
            FieldId::ParquetMaxRowGroupRows => self.config.parquet.settings.max_row_group_rows.clone(),
            FieldId::ParquetOutputBufferBytes => self.config.parquet.settings.output_buffer_bytes.clone(),
            FieldId::VortexTargetPartitionRows => self.config.vortex.settings.target_partition_rows.clone(),
            FieldId::VortexRowBlockSize => self.config.vortex.settings.row_block_size.clone(),
            FieldId::VortexOutputBufferBytes => self.config.vortex.settings.output_buffer_bytes.clone(),
            FieldId::ScaleFactor => self.config.scale_factor.clone(),
            FieldId::OutputPath => self.config.output_path.clone(),
            _ => String::new(),
        }
    }

    fn field_line(&self, field: FieldId, text: String) -> Line<'static> {
        let prefix = if self.focus == field { "> " } else { "  " };
        Line::from(format!("{prefix}{text}"))
    }

    fn toggle_mark(&self, enabled: bool, supported: bool) -> &'static str {
        if !supported {
            "disabled"
        } else if enabled {
            "x"
        } else {
            " "
        }
    }

    fn format_summary(&self, formats: &[RunFormat]) -> String {
        if formats.is_empty() {
            return "none".to_string();
        }

        formats
            .iter()
            .map(|format| format.as_str().to_string())
            .collect::<Vec<_>>()
            .join(", ")
    }

    fn can_start_reason(&self) -> Option<String> {
        if self.editing.is_some() {
            return Some("finish editing first".to_string());
        }
        if self.run.is_some() {
            return Some("generation already running".to_string());
        }
        if let Some(error) = &self.backend_error {
            return Some(error.clone());
        }
        if self.backend_path.is_none() {
            return Some("backend path is unresolved".to_string());
        }
        if self.config.output_path.trim().is_empty() {
            return Some("output path is empty".to_string());
        }
        if self.config.snapshot().selected_formats().is_empty() {
            return Some("enable at least one supported format".to_string());
        }
        None
    }

    fn start_run(&mut self) {
        if let Some(reason) = self.can_start_reason() {
            self.status_message = Some(reason);
            return;
        }

        let snapshot = self.config.snapshot();
        let queue = snapshot.selected_formats();
        let backend_path = self.backend_path.clone().expect("validated backend path");
        let (tx, rx) = mpsc::channel();
        let current_format = queue[0];
        let worker = spawn_backend_worker(
            backend_path,
            current_format,
            snapshot.command_args_for(current_format),
            tx,
        );

        self.last_completed_formats.clear();
        self.last_error = None;
        self.last_run_snapshot = Some(snapshot.clone());
        self.quit_after_run = false;
        self.status_message = Some(format!("Started {}", current_format.title()));
        self.run = Some(RunState {
            snapshot,
            queue,
            current_index: 0,
            tables: fresh_table_map(),
            rx,
            worker: Some(worker),
        });
    }

    fn apply_progress_event(&mut self, event: ProgressEvent) {
        let Some(run) = self.run.as_mut() else {
            return;
        };

        match event {
            ProgressEvent::RunStarted {
                selected_formats,
                scale_factor,
                output_path,
            } => {
                self.status_message = Some(format!(
                    "Backend started: formats={} scale_factor={} output={}",
                    selected_formats.join(", "),
                    scale_factor,
                    output_path
                ));
            }
            ProgressEvent::FormatStarted { format, index, total_formats } => {
                self.status_message = Some(format!("Running {format} ({index}/{total_formats})"));
            }
            ProgressEvent::TableStarted {
                format,
                table,
                total_rows,
                part_count,
            } => {
                if format != run.current_format().as_str() {
                    return;
                }
                let status = run.tables.entry(table).or_default();
                status.kind = TableStatusKind::Running;
                status.total_rows = total_rows;
                status.completed_parts = 0;
                status.detail = Some(format!("{part_count} parts"));
            }
            ProgressEvent::TableProgress {
                format,
                table,
                rows_written,
                total_rows,
                completed_parts,
            } => {
                if format != run.current_format().as_str() {
                    return;
                }
                let status = run.tables.entry(table).or_default();
                status.kind = TableStatusKind::Running;
                status.rows_written = rows_written;
                status.total_rows = total_rows;
                status.completed_parts = completed_parts;
            }
            ProgressEvent::TableFinished {
                format,
                table,
                rows_written,
                total_rows,
            } => {
                if format != run.current_format().as_str() {
                    return;
                }
                let status = run.tables.entry(table).or_default();
                status.kind = TableStatusKind::Done;
                status.rows_written = rows_written;
                status.total_rows = total_rows;
                status.detail = None;
            }
            ProgressEvent::TableFailed {
                format,
                table,
                error,
            } => {
                if format != run.current_format().as_str() {
                    return;
                }
                let status = run.tables.entry(table).or_default();
                status.kind = TableStatusKind::Error;
                status.detail = Some(error.clone());
                self.last_error = Some(error);
            }
            ProgressEvent::FormatFinished { format, success } => {
                self.status_message = Some(format!("Format {format} finished: {}", if success { "ok" } else { "failed" }));
            }
            ProgressEvent::RunFinished { success } => {
                self.status_message = Some(format!("Backend run finished: {}", if success { "ok" } else { "failed" }));
            }
        }
    }

    fn finish_current_worker(&mut self, outcome: ProcessOutcome) {
        let Some(mut run) = self.run.take() else {
            return;
        };

        if let Some(worker) = run.worker.take() {
            let _ = worker.join();
        }

        let finished_format = outcome.format;
        if outcome.success {
            self.last_completed_formats.push(finished_format);
            run.current_index += 1;

            if run.current_index < run.queue.len() {
                let current_format = run.current_format();
                let backend_path = self.backend_path.clone().expect("backend path exists");
                let (tx, rx) = mpsc::channel();
                let worker = spawn_backend_worker(
                    backend_path,
                    current_format,
                    run.snapshot.command_args_for(current_format),
                    tx,
                );
                run.tables = fresh_table_map();
                run.rx = rx;
                run.worker = Some(worker);
                self.status_message = Some(format!("Started {}", current_format.title()));
                self.run = Some(run);
            } else {
                self.status_message = Some("Generation finished successfully".to_string());
                if self.quit_after_run {
                    self.should_quit = true;
                }
            }
            return;
        }

        self.last_error = Some(format_outcome_error(&outcome));
        self.status_message = Some("Generation failed".to_string());
        if self.quit_after_run {
            self.should_quit = true;
        }
    }
}

fn fresh_table_map() -> HashMap<String, TableStatus> {
    let mut map = HashMap::new();
    for table in TABLE_ORDER {
        map.insert((*table).to_string(), TableStatus::default());
    }
    map
}

fn format_outcome_error(outcome: &ProcessOutcome) -> String {
    if let Some(parse_error) = &outcome.parse_error {
        return parse_error.clone();
    }
    if !outcome.stderr.trim().is_empty() {
        return outcome.stderr.trim().to_string();
    }
    match outcome.exit_code {
        Some(code) => format!("backend exited with status {code}"),
        None => "backend terminated without an exit code".to_string(),
    }
}

pub fn run_app(app: &mut App, terminal: &mut ratatui::DefaultTerminal) -> Result<()> {
    loop {
        app.pump_messages();
        terminal.draw(|frame| app.draw(frame))?;

        if app.should_quit() {
            break;
        }

        if crossterm::event::poll(Duration::from_millis(120))? {
            if let crossterm::event::Event::Key(key) = crossterm::event::read()? {
                if key.kind == crossterm::event::KeyEventKind::Press {
                    app.handle_key(key);
                }
            }
        } else {
            app.tick();
        }
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn make_test_app() -> App {
        App {
            tab: SidebarTab::Formats,
            focus: FieldId::ParquetEnabled,
            editing: None,
            config: AppConfig::from_supported_formats(&[RunFormat::Parquet, RunFormat::Vortex]),
            backend_path: Some(PathBuf::from("./build-vortex/schengen_main")),
            backend_error: None,
            supported_formats: vec![RunFormat::Parquet, RunFormat::Vortex],
            run: None,
            spinner_index: 0,
            should_quit: false,
            quit_after_run: false,
            status_message: None,
            last_completed_formats: Vec::new(),
            last_run_snapshot: None,
            last_error: None,
        }
    }

    #[test]
    fn snapshot_builds_parquet_command_args() {
        let mut app = make_test_app();
        app.config.vortex.enabled = false;
        app.config.parquet.settings.use_threads = true;

        let snapshot = app.config.snapshot();
        let args = snapshot.command_args_for(RunFormat::Parquet);

        assert!(args.contains(&"--progress-json".to_string()));
        assert!(args.contains(&"--output-format".to_string()));
        assert!(args.contains(&"parquet".to_string()));
        assert!(args.contains(&"--parquet-use-threads".to_string()));
        assert!(args.contains(&"--parquet-compression".to_string()));
    }

    #[test]
    fn snapshot_builds_vortex_command_args() {
        let mut app = make_test_app();
        app.config.parquet.enabled = false;
        app.config.vortex.enabled = true;

        let snapshot = app.config.snapshot();
        let args = snapshot.command_args_for(RunFormat::Vortex);

        assert!(args.contains(&"--output-format".to_string()));
        assert!(args.contains(&"vortex".to_string()));
        assert!(args.contains(&"--vortex-target-partition-rows".to_string()));
        assert!(args.contains(&"--vortex-row-block-size".to_string()));
    }

    #[test]
    fn selected_formats_use_fixed_order() {
        let app = make_test_app();
        let snapshot = app.config.snapshot();
        assert_eq!(snapshot.selected_formats(), vec![RunFormat::Parquet, RunFormat::Vortex]);
    }

    #[test]
    fn apply_progress_event_updates_table_state() {
        let mut app = make_test_app();
        let (_tx, rx) = mpsc::channel();
        app.run = Some(RunState {
            snapshot: app.config.snapshot(),
            queue: vec![RunFormat::Parquet],
            current_index: 0,
            tables: fresh_table_map(),
            rx,
            worker: None,
        });

        app.apply_progress_event(ProgressEvent::TableStarted {
            format: "parquet".to_string(),
            table: "supplier".to_string(),
            total_rows: 100,
            part_count: 2,
        });
        app.apply_progress_event(ProgressEvent::TableProgress {
            format: "parquet".to_string(),
            table: "supplier".to_string(),
            rows_written: 50,
            total_rows: 100,
            completed_parts: 1,
        });
        app.apply_progress_event(ProgressEvent::TableFinished {
            format: "parquet".to_string(),
            table: "supplier".to_string(),
            rows_written: 100,
            total_rows: 100,
        });

        let run = app.run.as_ref().expect("run exists");
        let supplier = run.tables.get("supplier").expect("supplier row exists");
        assert_eq!(supplier.kind, TableStatusKind::Done);
        assert_eq!(supplier.rows_written, 100);
        assert_eq!(supplier.total_rows, 100);
    }
}
