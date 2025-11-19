# Compile / Build (short & direct)

## Dependencies (install first)

### Arch
sudo pacman -Syu  
sudo pacman -S --needed base-devel git  

### Debian / Ubuntu
sudo apt update  
sudo apt install -y build-essential git  

### Fedora
sudo dnf upgrade --refresh  
sudo dnf groupinstall -y "Development Tools"  
sudo dnf install -y git  

### Void Linux
sudo xbps-install -S  
sudo xbps-install -y base-devel git  

> Note: cjson.c and cjson.h must be in the project folder.  
> If not included, download them from the cJSON repo before compiling.

---

## Clone, build and run

git clone https://github.com/matheusdev-45/taskline.git  
cd taskline  
make  
./taskline help  

---

## Install globally (optional)

sudo make install  
taskline help

## Uninstall:

sudo make uninstall

---

## Quick troubleshooting
- If gcc is missing: install dependencies listed above.  
- If tasks.json not found: program creates it automatically on first run.
