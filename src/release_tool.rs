use crate::extend::{read_blake3, Blake3Path};
mod extend;
mod global;
mod release_definition;

use crate::global::UPDATE_INFO_ASSET_NAME;
use crate::release_definition::{UpdateArchiveDto, UpdateDataDto, UpdateFileDto};
use std::fs::File;
use std::io::BufReader;
use std::path::{Path, PathBuf};
use std::{env, fs};
use zip::ZipArchive;

fn visit_archive(
    file: &Path,
    archive_name: &str,
    relative_path: &str,
    data: &mut UpdateDataDto,
) -> Result<(), Box<dyn std::error::Error>> {
    println!("Visiting archive \"{relative_path}\"");

    let file = File::open(file)?;
    let mut buf_reader = BufReader::new(file);
    let mut zip = ZipArchive::new(&mut buf_reader)?;
    let zip_file_count = zip.len();

    for file_index in 0..zip_file_count {
        let mut file_in_zip = zip.by_index(file_index)?;
        if !file_in_zip.is_file() {
            continue;
        }

        let file_path = file_in_zip
            .enclosed_name()
            .ok_or("No filename in zip")?
            .to_str()
            .ok_or("Could not convert filename to string in zip")?
            .replace("\\", "/");

        println!("Visiting file {file_path} in archive \"{relative_path}\"");

        let size = file_in_zip.size() as u32;
        let blake3 = read_blake3(&mut file_in_zip)?;

        data.files.push(UpdateFileDto {
            blake3,
            size,
            path: file_path,
            asset_name: None,
            archive: Some(String::from(archive_name)),
        });
    }

    Ok(())
}

fn visit_dir_file(
    file: &Path,
    file_name: String,
    relative_path: String,
    data: &mut UpdateDataDto,
) -> Result<(), Box<dyn std::error::Error>> {
    if relative_path == UPDATE_INFO_ASSET_NAME {
        println!("File \"{relative_path}\" is the update info definition and ignored");
        return Ok(());
    }

    println!("Visiting file \"{relative_path}\"");

    let metadata = file.metadata()?;
    let size = metadata.len() as u32;
    let blake3 = file.get_blake3()?;
    let is_zip = file
        .extension()
        .map(|extension| extension.eq_ignore_ascii_case("zip"))
        .unwrap_or(false);

    if is_zip {
        visit_archive(file, &file_name, &relative_path, data)?;
        data.archives.push(UpdateArchiveDto {
            blake3,
            size,
            name: file_name,
        });
    } else {
        data.files.push(UpdateFileDto {
            blake3,
            size,
            path: relative_path,
            asset_name: Some(file_name),
            archive: None,
        });
    }

    Ok(())
}

fn visit_dir(
    dir: &Path,
    relative_path: &str,
    data: &mut UpdateDataDto,
) -> Result<(), Box<dyn std::error::Error>> {
    println!("Visiting dir \"{relative_path}\"");

    let dir_files = fs::read_dir(dir)?;
    for maybe_file in dir_files {
        let file = maybe_file?;
        let file_path = file.path();
        let file_type = file.file_type()?;

        let file_name = String::from(
            file_path
                .file_name()
                .ok_or("File must have a name")?
                .to_str()
                .ok_or("Failed to convert file name to str")?,
        );
        let file_relative_path = if relative_path.is_empty() {
            String::from(&file_name)
        } else {
            format!("{relative_path}/{file_name}")
        };

        if file_type.is_file() {
            visit_dir_file(&file_path, file_name, file_relative_path, data)?;
        } else if file_type.is_dir() {
            visit_dir(&file_path, &file_relative_path, data)?;
        }
    }

    Ok(())
}

fn build_release_definition(dir: &Path) -> Result<UpdateDataDto, Box<dyn std::error::Error>> {
    let mut data = UpdateDataDto {
        archives: vec![],
        files: vec![],
    };

    visit_dir(dir, "", &mut data)?;

    Ok(data)
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        eprintln!("Usage: <path>");
        return Ok(());
    }

    let path_str = args[1].as_str();
    let path = PathBuf::from(path_str);

    if path.metadata().map(|m| !m.is_dir()).unwrap_or(true) {
        return Err(Box::from(format!("Must be a directory: {path_str}")));
    }

    let update_data = build_release_definition(&path)?;
    let update_data_str = serde_json::to_string(&update_data)?;

    let definition_file = path.join(UPDATE_INFO_ASSET_NAME);
    fs::write(&definition_file, update_data_str.as_str())?;

    Ok(())
}
