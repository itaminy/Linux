TARGET = kubsh
BUILD_DIR = build

all: build

build:
	mkdir -p $(BUILD_DIR)
	gcc -Wall -pthread -o $(BUILD_DIR)/$(TARGET) kubsh.c vfs_manager.c -lfuse3 -lreadline

run: build
	./$(BUILD_DIR)/$(TARGET)

package: build
	mkdir -p kubsh-package/DEBIAN
	mkdir -p kubsh-package/usr/bin
	cp $(BUILD_DIR)/$(TARGET) kubsh-package/usr/bin/
	chmod +x kubsh-package/usr/bin/$(TARGET)
	echo "Package: kubsh" > kubsh-package/DEBIAN/control
	echo "Version: 1.0" >> kubsh-package/DEBIAN/control
	echo "Section: utils" >> kubsh-package/DEBIAN/control
	echo "Priority: optional" >> kubsh-package/DEBIAN/control
	echo "Architecture: amd64" >> kubsh-package/DEBIAN/control
	echo "Maintainer: Mitohondriyaa" >> kubsh-package/DEBIAN/control
	echo "Description: Kubsh" >> kubsh-package/DEBIAN/control
	dpkg-deb --build kubsh-package
	mv kubsh-package.deb kubsh.deb

clean:
	rm -rf $(BUILD_DIR) kubsh.deb kubsh-package

.PHONY: all build run package clean
