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
	rm -rf "$(OUTPUT)/$(ZIP_NAME).zip" "$(ZIP_NAME)"
	mkdir -p "$(ZIP_NAME)"
	for p in $(ZIP_FILES); do \
		mkdir -p "$(ZIP_NAME)/$$(dirname $$p)"; \
		cp -Rp $$p "$(ZIP_NAME)/$$(dirname $$p)"; \
	done
	mkdir -p "$(OUTPUT)"
	zip -9r "$(OUTPUT)/$(ZIP_NAME).zip" "$(ZIP_NAME)"
	rm -rf "$(ZIP_NAME)"

clean:
	$(MAKE) -f Makefile.mingw clean
	$(MAKE) -f Makefile.wine clean
	rm -rf "$(OUTPUT)/$(ZIP_NAME).zip" "$(ZIP_NAME)"
