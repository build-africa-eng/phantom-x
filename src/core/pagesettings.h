#ifndef PAGESETTINGS_H
#define PAGESETTINGS_H

// Constants for WebPage and Config settings keys using #define macros
#define PAGE_SETTINGS_USER_AGENT "userAgent"
#define PAGE_SETTINGS_VIEWPORT_SIZE "viewportSize"
#define PAGE_SETTINGS_CLIP_RECT "clipRect"
#define PAGE_SETTINGS_SCROLL_POSITION "scrollPosition"
#define PAGE_SETTINGS_ZOOM_FACTOR "zoomFactor"
#define PAGE_SETTINGS_CUSTOM_HEADERS "customHeaders"
#define PAGE_SETTINGS_NAVIGATION_LOCKED "navigationLocked"
#define PAGE_SETTINGS_PAPER_SIZE "paperSize"

// Render-related
#define PAGE_SETTINGS_ONLY_VIEWPORT "onlyViewport" // For render() options
#define PAGE_SETTINGS_FORMAT "format" // For render() options

// Network/Cache related
#define PAGE_SETTINGS_DISK_CACHE_ENABLED "diskCacheEnabled"
#define PAGE_SETTINGS_MAX_DISK_CACHE_SIZE "maxDiskCacheSize"
#define PAGE_SETTINGS_DISK_CACHE_PATH "diskCachePath"
#define PAGE_SETTINGS_IGNORE_SSL_ERRORS "ignoreSslErrors"
#define PAGE_SETTINGS_SSL_PROTOCOL "sslProtocol"
#define PAGE_SETTINGS_SSL_CIPHERS "sslCiphers"
#define PAGE_SETTINGS_SSL_CERTIFICATES_PATH "sslCertificatesPath"
#define PAGE_SETTINGS_SSL_CLIENT_CERTIFICATE_FILE "sslClientCertificateFile"
#define PAGE_SETTINGS_SSL_CLIENT_KEY_FILE "sslClientKeyFile"
#define PAGE_SETTINGS_SSL_CLIENT_KEY_PASSPHRASE "sslClientKeyPassphrase"
#define PAGE_SETTINGS_RESOURCE_TIMEOUT "resourceTimeout"
#define PAGE_SETTINGS_MAX_AUTH_ATTEMPTS "maxAuthAttempts"

// JavaScript/Security related
#define PAGE_SETTINGS_JAVASCRIPT_ENABLED "javascriptEnabled"
#define PAGE_SETTINGS_WEB_SECURITY "webSecurityEnabled"
#define PAGE_SETTINGS_WEBG_ENABLED "webGLEnabled" // Standard name for WebGL
#define PAGE_SETTINGS_JAVASCRIPT_CAN_OPEN_WINDOWS "javascriptCanOpenWindows"
#define PAGE_SETTINGS_JAVASCRIPT_CAN_CLOSE_WINDOWS "javascriptCanCloseWindows"
#define PAGE_SETTINGS_LOCAL_TO_REMOTE_URL_ACCESS_ENABLED "localToRemoteUrlAccessEnabled"
#define PAGE_SETTINGS_AUTO_LOAD_IMAGES "autoLoadImages"

// Local/Offline Storage
#define PAGE_SETTINGS_LOCAL_STORAGE_PATH "localStoragePath"
#define PAGE_SETTINGS_LOCAL_STORAGE_QUOTA "localStorageQuota"
#define PAGE_SETTINGS_OFFLINE_STORAGE_PATH "offlineStoragePath"
#define PAGE_SETTINGS_OFFLINE_STORAGE_QUOTA "offlineStorageQuota"

// Print Settings (PDF specific)
#define PAGE_SETTINGS_PRINT_HEADER "printHeader"
#define PAGE_SETTINGS_PRINT_FOOTER "printFooter"

#endif // PAGESETTINGS_H
