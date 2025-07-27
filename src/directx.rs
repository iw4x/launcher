use serde::{Deserialize, Serialize};
use std::path::Path;
#[cfg(target_os = "windows")]
use windows_sys::{
    core::*, Win32::Foundation::*, Win32::System::Threading::*, Win32::UI::WindowsAndMessaging::*,
};

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
            std::fs::create_dir_all(&install_dir)
                .map_err(DirectXError::InvalidInstallDirectory)?;
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
