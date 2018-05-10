VERSION :=	$(shell git describe --tags)
ZIP_NAME ?=	csr-spi-ftdi-$(VERSION)
ZIP_FILES +=	lib-win32 lib-wine-linux README.md hardware misc utils
OUTPUT=release

all: win32 wine

win32::
	$(MAKE) -f Makefile.mingw all

wine::
	$(MAKE) -f Makefile.wine all

zip: all $(OUTPUT)/$(ZIP_NAME).zip

$(OUTPUT)/$(ZIP_NAME).zip:
	mkdir -p "$(OUTPUT)"
	rm -rf "$(OUTPUT)/$(ZIP_NAME).zip" "$(OUTPUT)/$(ZIP_NAME)"
	mkdir -p "$(OUTPUT)/$(ZIP_NAME)"
	for p in $(ZIP_FILES); do \
		mkdir -p "$(OUTPUT)/$(ZIP_NAME)/$$(dirname $$p)"; \
		cp -Rp $$p "$(OUTPUT)/$(ZIP_NAME)/$$(dirname $$p)"; \
	done
	(cd "$(OUTPUT)" && zip -9r "$(ZIP_NAME).zip" "$(ZIP_NAME)")
	rm -rf "$(OUTPUT)/$(ZIP_NAME)"

clean:
	$(MAKE) -f Makefile.mingw clean
	$(MAKE) -f Makefile.wine clean
	rm -rf "$(OUTPUT)/$(ZIP_NAME)" "$(OUTPUT)/$(ZIP_NAME).zip"
