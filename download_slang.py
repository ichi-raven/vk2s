import os
import sys
import platform
import subprocess

if len(sys.argv) < 3:
    print("Usage: python download_slang.py <install_dir> <version>")
    sys.exit(1)

install_dir = sys.argv[1]

version = sys.argv[2]

current_os = platform.system()

download_dir = os.path.join(install_dir, "download")
os.makedirs(download_dir, exist_ok=True)

zip_file_path = os.path.join(install_dir, f"slang-{version}.zip")

if current_os == "Windows":

    url = f"https://github.com/shader-slang/slang/releases/download/v{version}/slang-{version}-windows-x86_64.zip"
    print(f"Downloading from {url}...")
    subprocess.run(["curl", "-L", url, "-o", zip_file_path], check=True)
elif current_os == "Linux":

    url = f"https://github.com/shader-slang/slang/releases/download/v{version}/slang-{version}-linux-x86_64.zip"
    print(f"Downloading from {url}...")
    subprocess.run(["wget", url, "-O", zip_file_path], check=True)
else:
    print("Unsupported OS.")
    sys.exit(1)

print(f"Downloaded: {zip_file_path}")

os.makedirs(install_dir, exist_ok=True)

print(f"Extracting to {install_dir}...")

if current_os == "Windows":
    subprocess.run(["powershell", "-Command", f"Expand-Archive -Path '{zip_file_path}' -DestinationPath '{install_dir}' -Force"], check=True)
elif current_os == "Linux":
    subprocess.run(["unzip", zip_file_path, "-d", install_dir], check=True)
else:
    print("Unsupported OS.")
    sys.exit(1)

print(f"Extracted to: {install_dir}")

def remove_directory(path):
    for root, dirs, files in os.walk(path, topdown=False):
        for name in files:
            os.remove(os.path.join(root, name)) 
        for name in dirs:
            os.rmdir(os.path.join(root, name))  
    os.rmdir(path)  


remove_directory(download_dir)
print(f"Removed download directory: {download_dir}")
