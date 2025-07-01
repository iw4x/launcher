use std::{
    cmp::min,
    fs::File,
    io::Write,
    path::PathBuf,
    time::{Duration, Instant},
};

use futures_util::StreamExt;
use indicatif::ProgressBar;
use once_cell::sync::Lazy;
use reqwest::{header::HeaderMap, Client};
use serde_json::Value;

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
        .map_err(|e| format!("Failed to send request to {}: {}", url, e))?;

    res.text()
        .await
        .map_err(|e| format!("Failed to get body: {}", e).into())
}

/// check if server is using Cloudflare based on headers
fn is_cloudflare(headers: &HeaderMap) -> bool {
    headers.contains_key("cf-ray")
        || headers.contains_key("cf-cache-status")
        || headers
            .get("server")
            .is_some_and(|v| v.as_bytes().starts_with(b"cloudflare"))
}

/// get url and track rating info
pub async fn rating_request(
    url: &str,
    timeout: Duration,
) -> Result<(Duration, bool), Box<dyn std::error::Error>> {
    let start = Instant::now();
    let res = HTTP_CLIENT.get(url).timeout(timeout).send().await;
    let latency = start.elapsed();

    let res = res.map_err(|e| format!("Request to {} failed: {} (after {:?})", url, e, latency))?;

    let headers = res.headers().clone();
    let is_cloudflare = is_cloudflare(&headers);

    res.text().await?;

    Ok((latency, is_cloudflare))
}

/// retrieve user asn and region
pub async fn get_location_info() -> (u32, String) {
    let response = get_body_string(crate::global::IP2ASN_URL).await;
    if let Ok(as_data_str) = response {
        if let Ok(as_data) = serde_json::from_str::<Value>(&as_data_str) {
            let as_number = as_data
                .get("as_number")
                .and_then(|v| v.as_u64())
                .unwrap_or(0) as u32;
            let region = as_data
                .get("region")
                .and_then(|v| v.as_str())
                .unwrap_or("Unknown")
                .to_string();
            return (as_number, region);
        }
    }
    (0, "Unknown".to_string())
}

/// download file in chunks with progress bars
pub async fn download_file_progress(
    file_pb: &ProgressBar,
    total_pb: &ProgressBar,
    url: &str,
    path: &PathBuf,
    size: u64,
    start_position: u64,
    relative_path: &str,
) -> Result<(), Box<dyn std::error::Error>> {
    let res = HTTP_CLIENT
        .get(url)
        .send()
        .await
        .map_err(|_| format!("Failed to GET from '{url}'"))?;

    let file_size = res.content_length().unwrap_or(size);

    log::debug!(
        "Starting download of {} ({})",
        relative_path,
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
        .map_err(|e| format!("Failed to GET from '{url}': {}", e))?;

    let bytes = res
        .bytes()
        .await
        .map_err(|e| format!("Failed to get bytes: {}", e))?;

    std::fs::write(path, bytes).map_err(|e| format!("Failed to write file: {}", e).into())
}
