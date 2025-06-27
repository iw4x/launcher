use std::collections::HashMap;
#[cfg(unix)]
use std::fs;

use colored::{ColoredString, Colorize};
use once_cell::sync::Lazy;

pub struct PrintPrefix {
    pub text: ColoredString,
    pub padding: usize,
}

impl PrintPrefix {
    pub fn formatted(&self) -> String {
        format!("[{}]{:width$}", self.text, "", width = self.padding)
    }
}

static PREFIXES: Lazy<HashMap<&'static str, PrintPrefix>> = Lazy::new(|| {
    HashMap::from([
        (
            "info",
            PrintPrefix {
                text: "Info".bright_green(),
                padding: 4,
            },
        ),
        (
            "updated",
            PrintPrefix {
                text: "Updated".bright_green(),
                padding: 1,
            },
        ),
        (
            "checked",
            PrintPrefix {
                text: "Checked".green(),
                padding: 1,
            },
        ),
        (
            "removed",
            PrintPrefix {
                text: "Removed".red(),
                padding: 1,
            },
        ),
        (
            "error",
            PrintPrefix {
                text: "Error".bright_red(),
                padding: 3,
            },
        ),
        (
            "renamed",
            PrintPrefix {
                text: "Renamed".green(),
                padding: 1,
            },
        ),
    ])
});

pub fn prefix(tag_name: &str) -> String {
    PREFIXES
        .get(tag_name)
        .map_or_else(|| tag_name.to_string(), |tag| tag.formatted())
}

#[macro_export]
macro_rules! println_info {
    ($($arg:tt)*) => {{
        println!("{}", format!("{}{}", $crate::misc::prefix("info"), format!($($arg)*)));
        log::info!($($arg)*);
    }}
}

#[macro_export]
macro_rules! println_error {
    ($($arg:tt)*) => {{
        eprintln!("{}", format!("{}{}", $crate::misc::prefix("error"), format!($($arg)*)));
        log::error!($($arg)*);
    }}
}

pub fn human_readable_bytes(bytes: u64) -> String {
    let mut bytes = bytes as f64;
    let mut i = 0;
    const UNITS: [&str; 9] = ["B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"];
    while bytes >= 1024.0 {
        bytes /= 1024.0;
        i += 1;
    }
    format!("{bytes:.2}{}", UNITS[i])
}

#[cfg(unix)]
pub fn is_program_in_path(program: &str) -> bool {
    std::env::var_os("PATH")
        .and_then(|paths| {
            paths.to_str().map(|paths| {
                paths
                    .split(':')
                    .any(|dir| fs::metadata(format!("{}/{}", dir, program)).is_ok())
            })
        })
        .unwrap_or(false)
}

pub fn stdin() {
    let mut input = String::new();
    std::io::stdin().read_line(&mut input).unwrap();
}

pub fn enter_exit(code: i32) {
    println!("Press Enter to exit..");
    stdin();
    std::process::exit(code);
}

pub fn random_string(length: usize) -> String {
    let mut rng = rand::rng();
    let chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    (0..length)
        .map(|_| {
            chars
                .chars()
                .nth(rand::Rng::random_range(&mut rng, 0..chars.len()))
                .unwrap()
        })
        .collect()
}
