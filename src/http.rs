use std::{cmp::min, fs::File, io::Write, path::PathBuf};

use futures_util::StreamExt;
use indicatif::ProgressBar;
use once_cell::sync::Lazy;
use reqwest::Client;

use crate::{extend::CutePath, misc};

/// shared HTTP client
static HTTP_CLIENT: Lazy<Client> = Lazy::new(|| {
    reqwest::Client::builder()
        .user_agent(crate::global::USER_AGENT.as_str())
        .build()
        .expect("Failed to build HTTP client")
});

pub async fn get_body_string(url: &str) -> Result<String, Box<dyn std::error::Error>> {
    let request = HTTP_CLIENT.get(url).timeout(crate::global::HTTP_TIMEOUT);

    let res = request
        .send()
        .await
        .map_err(|e| format!("Failed to send request to {url}: {e}"))?;

    res.text()
        .await
        .map_err(|e| format!("Failed to get body: {e}").into())
}

/// download file in chunks with progress bars
pub async fn download_file_progress(
    file_pb: &ProgressBar,
    total_pb: &ProgressBar,
    url: &str,
    path: &PathBuf,
    size: u64,
    start_position: u64,
    file_name: &str,
) -> Result<(), Box<dyn std::error::Error>> {
    let res = HTTP_CLIENT
        .get(url)
        .send()
        .await
        .map_err(|_| format!("Failed to GET from '{url}'"))?;

    let file_size = res.content_length().unwrap_or(size);

    log::debug!(
        "Starting download of {} ({})",
        file_name,
        misc::human_readable_bytes(file_size)
    );

    let mut file =
        File::create(path).map_err(|_| format!("Failed to create file '{}'", path.cute_path()))?;
    let mut downloaded: u64 = 0;
    let mut stream = res.bytes_stream();

    while let Some(item) = stream.next().await {
        let chunk = item.map_err(|e| format!("Error while downloading file: {e}"))?;
        file.write_all(&chunk)
            .map_err(|e| format!("Error while writing to file: {e}"))?;

        downloaded = min(downloaded + (chunk.len() as u64), file_size);

        if let Some(file_name) = path.file_name().and_then(|n| n.to_str()) {
            file_pb.set_message(file_name.to_string());
        }
        file_pb.set_position(downloaded);

        total_pb.set_position(start_position + downloaded);
    }

    file_pb.set_message(String::default());

    // not really necessary, but i'll leave it here for "now"
    // let msg = format!("{}{}", misc::prefix("updated"), relative_path);
    // total_pb.println(&msg);

    Ok(())
}

/// download to file
pub async fn download_file(
    url: &str,
    path: &std::path::Path,
) -> Result<(), Box<dyn std::error::Error>> {
    let res = HTTP_CLIENT
        .get(url)
        .send()
        .await
        .map_err(|e| format!("Failed to GET from '{url}': {e}"))?;

    let bytes = res
        .bytes()
        .await
        .map_err(|e| format!("Failed to get bytes: {e}"))?;

    std::fs::write(path, bytes).map_err(|e| format!("Failed to write file: {e}").into())
}
