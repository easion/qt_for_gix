CONFIG +=link_pkgconfig
PKGCONFIG = freetype2
DEFINES += QT_NO_FONTCONFIG
LIBS_PRIVATE +=  -L/usr/local/lib -lgi #-lfontconfig
