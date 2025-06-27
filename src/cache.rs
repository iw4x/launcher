use std::{collections::HashMap, fs, path::Path};

pub fn get_cache(dir: &Path) -> HashMap<String, String> {
    let cache_path = dir.join("cache.json");
    let cache_content = fs::read_to_string(cache_path).unwrap_or_default();
    if cache_content.trim().is_empty() {
        HashMap::new()
    } else {
        serde_json::from_str(&cache_content).unwrap_or_default()
    }
}

pub fn save_cache(dir: &Path, cache: HashMap<String, String>) {
    let cache_path = dir.join("cache.json");
    let cache_serialized = match serde_json::to_string_pretty(&cache) {
        Ok(s) => s,
        Err(e) => {
            log::error!("Failed to serialize cache: {}", e);
            return;
        }
    };
    fs::write(cache_path, cache_serialized).unwrap_or_else(|e| {
        log::error!("Failed to save cache: {}", e);
    });
}
