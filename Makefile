CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_GNU_SOURCE
LDFLAGS = -lreadline
TARGET = kubsh

VERSION = 1.0.0
PACKAGE_NAME = kubsh
BUILD_DIR = build
DEB_DIR = $(BUILD_DIR)/$(PACKAGE_NAME)_$(VERSION)_amd64
DEB_DEPENDS = libreadline8

all: $(TARGET)

$(TARGET): kubsh.c
	$(CC) $(CFLAGS) kubsh.c -o $(TARGET) $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) *.deb

run: $(TARGET)
	./$(TARGET)

prepare-deb: $(TARGET)
	@echo "Подготовка структуры для deb-пакета..."
	@rm -rf $(BUILD_DIR)
	@mkdir -p $(DEB_DIR)/DEBIAN
	@mkdir -p $(DEB_DIR)/usr/local/bin
	@mkdir -p $(DEB_DIR)/usr/share/doc/$(PACKAGE_NAME)
	
	@cp $(TARGET) $(DEB_DIR)/usr/local/bin/
	@chmod 755 $(DEB_DIR)/usr/local/bin/$(TARGET)
	
	@echo "Package: $(PACKAGE_NAME)" > $(DEB_DIR)/DEBIAN/control
	@echo "Version: $(VERSION)" >> $(DEB_DIR)/DEBIAN/control
	@echo "Section: utils" >> $(DEB_DIR)/DEBIAN/control
	@echo "Priority: optional" >> $(DEB_DIR)/DEBIAN/control
	@echo "Architecture: amd64" >> $(DEB_DIR)/DEBIAN/control
	@echo "Maintainer: $(USER) <$(USER)@localhost>" >> $(DEB_DIR)/DEBIAN/control
	@echo "Description: Custom shell" >> $(DEB_DIR)/DEBIAN/control
	@echo " KubSH is a custom shell implementation" >> $(DEB_DIR)/DEBIAN/control
	@echo "Depends: $(DEB_DEPENDS)" >> $(DEB_DIR)/DEBIAN/control
	
	@echo "MIT License" > $(DEB_DIR)/usr/share/doc/$(PACKAGE_NAME)/copyright

deb: prepare-deb
	@echo "Сборка deb-пакета..."
	dpkg-deb --build --root-owner-group $(DEB_DIR)
	@cp $(BUILD_DIR)/$(PACKAGE_NAME)_$(VERSION)_amd64.deb kubsh.deb
	@echo "Пакет создан: kubsh.deb"

test: deb
	@echo "Запуск теста в Docker..."
	sudo docker run -v $(PWD):/mnt tyvik/kubsh_test:master

.PHONY: all clean run deb prepare-deb test
