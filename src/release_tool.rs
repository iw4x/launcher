use crate::extend::{read_blake3, Blake3Path};
mod extend;
#[allow(dead_code)]
mod global;
mod release_definition;

use crate::global::UPDATE_INFO_ASSET_NAME;
use crate::release_definition::{UpdateArchiveDto, UpdateDataDto, UpdateFileDto};
use clap::Parser;
use std::fs;
use std::fs::File;
use std::io::BufReader;
use std::path::{Path, PathBuf};
use zip::ZipArchive;

#[derive(Parser, Debug)]
#[command(author, version, about, long_about = None)]
struct Args {
    /// Rename files to technical names so users are not tempted to download them
    /// as they are only meant for the launcher
    #[arg(long = "rename-files")]
    rename_files: bool,

    /// The folder to create an update.json for.
    /// The resulting `update.json` file is placed in that folder as well
    path: PathBuf,
}

fn rename_update_file(
    file: &Path,
    keep_extension: bool,
) -> Result<PathBuf, Box<dyn std::error::Error>> {
    let file_name = file
        .file_name()
        .ok_or("File must have a name")?
        .to_str()
        .ok_or("Failed to convert file name to str")?;

    let updated_file_name = if keep_extension {
        String::from(file_name)
    } else {
        file_name.replace(".", "_")
    };

    let renamed_file_name = format!("__launcher_{updated_file_name}");
    let renamed_file_full_path = file
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .join(&renamed_file_name);

    fs::rename(file, &renamed_file_full_path)?;

    Ok(renamed_file_full_path)
}

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
    args: &Args,
    file: &Path,
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

    let update_file_path = if args.rename_files {
        rename_update_file(file, is_zip)?
    } else {
        PathBuf::from(file)
    };
    let file_name = update_file_path
        .file_name()
        .ok_or("File must have a name")?
        .to_str()
        .ok_or("Failed to convert file name to str")?
        .to_string();

    if is_zip {
        visit_archive(&update_file_path, &file_name, &relative_path, data)?;
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
    args: &Args,
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
            visit_dir_file(args, &file_path, file_relative_path, data)?;
        } else if file_type.is_dir() {
            visit_dir(args, &file_path, &file_relative_path, data)?;
        }
    }

    Ok(())
}

fn build_release_definition(
    args: &Args,
    dir: &Path,
) -> Result<UpdateDataDto, Box<dyn std::error::Error>> {
    let mut data = UpdateDataDto {
        archives: vec![],
        files: vec![],
    };

    visit_dir(args, dir, "", &mut data)?;

    Ok(data)
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();

    let path = &args.path;
    let path_str = path.to_str().ok_or("Failed to convert path to str")?;

    if path.metadata().map(|m| !m.is_dir()).unwrap_or(true) {
        return Err(Box::from(format!("Must be a directory: {path_str}")));
    }

    let update_data = build_release_definition(&args, path)?;
    let update_data_str = serde_json::to_string(&update_data)?;

    let definition_file = path.join(UPDATE_INFO_ASSET_NAME);
    fs::write(&definition_file, update_data_str.as_str())?;

    Ok(())
}
