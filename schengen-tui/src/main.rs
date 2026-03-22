mod app;
mod backend;

use std::path::PathBuf;

use anyhow::Result;

fn parse_args() -> Result<Option<PathBuf>> {
    let mut args = std::env::args_os().skip(1);
    let mut backend = None;

    while let Some(arg) = args.next() {
        if arg == "--help" {
            println!("Usage: schengen-tui [--backend PATH]");
            std::process::exit(0);
        }

        if arg == "--backend" {
            let Some(path) = args.next() else {
                anyhow::bail!("--backend requires a PATH argument");
            };
            backend = Some(PathBuf::from(path));
            continue;
        }

        anyhow::bail!("unknown argument: {:?}", arg);
    }

    Ok(backend)
}

fn main() -> Result<()> {
    let backend_override = parse_args()?;
    let mut app = app::App::new(backend_override);
    let mut terminal = ratatui::init();
    let result = app::run_app(&mut app, &mut terminal);
    ratatui::restore();
    result
}
