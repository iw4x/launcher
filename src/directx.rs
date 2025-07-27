use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};
#[cfg(target_os = "windows")]
use windows_sys::{
    core::*, Win32::Foundation::*, Win32::System::Threading::*, Win32::UI::WindowsAndMessaging::*,
};

use crate::{extend::CutePath, global::LAUNCHER_DIR, println_error};

///
pub mod config {
    ///
    pub const DIRECTX_WEB_RUNTIME_URL: &str = "https://download.microsoft.com/download/1/7/1/1718ccc4-6315-4d8e-9543-8e28a4e18c4c/dxwebsetup.exe";
    ///
    pub const DIRECTX_INSTALLER_NAME: &str = "dxwebsetup.exe";
    ///
    pub const DIRECTX_TEMP_DIR: &str = "directx_temp";
}

///
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum RuntimeEnvironment {
    /// Windows
    Windows,
    /// Wine compatibility
    Wine,
    /// Other platforms (Linux, macOS, etc.)
    Other,
}

///
#[derive(Debug, thiserror::Error)]
pub enum DirectXError {
    ///
    #[error("Failed to initialize DirectX runtime: {0}")]
    InitializationError(String),

    ///
    #[error("DirectX is not supported on {0:?}")]
    UnsupportedPlatform(RuntimeEnvironment),

    ///
    #[error("Invalid installation directory: {0}")]
    InvalidInstallDirectory(#[from] std::io::Error),

    //
    #[error("Failed to download DirectX runtime: {url} - {source}")]
    DownloadError {
        url: String,
        source: Box<dyn std::error::Error + Send + Sync>,
    },
}

///
pub type DirectXResult<T> = Result<T, DirectXError>;

#[derive(Debug)]
pub struct DirectX {
    ///
    environment: RuntimeEnvironment,
    ///
    install_dir: std::path::PathBuf,
}

///
impl DirectX {
    ///
    pub fn new<P: AsRef<Path>>(install_dir: P) -> DirectXResult<Self> {
        let install_dir = install_dir.as_ref().to_path_buf();
        if !install_dir.exists() {
            std::fs::create_dir_all(&install_dir).map_err(DirectXError::InvalidInstallDirectory)?;
        }

        let environment = Self::detect_environment();
        println!("Detected runtime environment: {:?}", environment);
        match environment {
            RuntimeEnvironment::Other => {
                return Err(DirectXError::UnsupportedPlatform(environment));
            }
            _ => {}
        }

        Ok(Self {
            environment,
            install_dir,
        })
    }

    ///
    pub async fn install_directx(&self) -> DirectXResult<()> {
        let runtime = self.download_runtime().await?;
        Ok(())
    }

    ///
    pub async fn download_runtime(&self) -> DirectXResult<PathBuf> {
        use std::fs;
        use std::io;
        use std::path::PathBuf;

        let temp_dir = self
            .install_dir
            .join(LAUNCHER_DIR)
            .join(config::DIRECTX_TEMP_DIR);

        if let Err(e) = fs::create_dir_all(&temp_dir) {
            println_error!(
                "Failed to create DirectX temporary directory '{}': {}",
                temp_dir.display(),
                e
            );
            return Err(DirectXError::InvalidInstallDirectory(e));
        }

        let installer_path = temp_dir.join(config::DIRECTX_INSTALLER_NAME);
        if installer_path.exists() {
            match fs::remove_file(&installer_path) {
                Ok(_) => println!(
                    "Removed stale DirectX installer at '{}'",
                    installer_path.display()
                ),
                Err(e) => {
                    println_error!(
                        "Failed to remove existing installer at '{}': {}",
                        installer_path.display(),
                        e
                    );
                    return Err(DirectXError::InvalidInstallDirectory(e));
                }
            }
        }

        println!("Preparing to download DirectX Web Runtime installer");
        println!("Target download path: {}", installer_path.display());
        println!("Download URL: {}", config::DIRECTX_WEB_RUNTIME_URL);

        crate::println_info!("Downloading DirectX Web Runtime...");

        // FIXME: HTTP 404 (Not Found) responses are currently not treated as
        // errors, which can result in an infinite retry loop if the resource
        // becomes permanently unavailable.
        //
        // In practice, this means that if the target URL is invalid, the
        // downloader will continue retrying indefinitely under the assumption
        // of a transient network issue or recoverable server-side failure.
        //
        match crate::http::download_file(config::DIRECTX_WEB_RUNTIME_URL, &installer_path).await {
            Ok(_) => {
                println!(
                    "Successfully downloaded DirectX installer to '{}'",
                    installer_path.cute_path()
                );
                Ok(installer_path)
            }
            Err(e) => {
                println_error!(
                    "Failed to download DirectX from '{}': {}",
                    config::DIRECTX_WEB_RUNTIME_URL,
                    e
                );
                Err(DirectXError::DownloadError {
                    url: config::DIRECTX_WEB_RUNTIME_URL.to_string(),
                    source: e.to_string().into(),
                })
            }
        }
    }

    ///
    pub fn environment(&self) -> RuntimeEnvironment {
        self.environment
    }

    ///
    pub fn install_dir(&self) -> &Path {
        &self.install_dir
    }

    ///
    fn detect_environment() -> RuntimeEnvironment {
        if cfg!(target_os = "windows") {
            if Self::is_wine_environment() {
                RuntimeEnvironment::Wine
            } else {
                RuntimeEnvironment::Windows
            }
        } else {
            RuntimeEnvironment::Other
        }
    }

    ///
    #[cfg(target_os = "windows")]
    fn is_wine_environment() -> bool {
        use windows_sys::Win32::Foundation::{GetLastError, HMODULE};
        use windows_sys::Win32::System::LibraryLoader::{GetModuleHandleA, GetProcAddress};

        unsafe {
            let ntdll_handle: HMODULE = GetModuleHandleA(b"ntdll.dll\0".as_ptr());
            if ntdll_handle == std::ptr::null_mut() {
                println!("Failed to get ntdll.dll handle: {}", GetLastError());
                return false;
            }

            let wine_get_version = GetProcAddress(ntdll_handle, b"wine_get_version\0".as_ptr());
            wine_get_version.is_some()
        }
    }

    ///
    #[cfg(not(target_os = "windows"))]
    fn is_wine_environment() -> bool {
        false
    }
}
