// Copyright (c) 2020, ellie/@ell1e & Horse64 Team (see AUTHORS.md),
// also see LICENSE.md file.
// SPDX-License-Identifier: BSD-2-Clause

#define _FILE_OFFSET_BITS 64
#define __USE_LARGEFILE64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#if defined(ANDROID) || defined(__ANDROID__)
#include <jni.h>
#endif
#include <stdarg.h>
#include <string.h>
#if defined(__unix__) || defined(__linux__) || defined(ANDROID) || defined(__ANDROID__) || defined(__APPLE__) || defined(__OSX__)
#if !defined(ANDROID) && !defined(__ANDROID__)
#include <pwd.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <shlobj.h>
#endif

#define MACOS_APPDATA_SUFFIX "/Libraries/Application Support/"
#define LINUX_APPDATA_SUFFIX "/.local/share/"
#define WINDOWS_APPDATA_SUFFIX "\\"


#include "filesys.h"
#include "stringhelpers.h"

int filesys_GetComponentCount(const char *path) {
    int i = 0;
    int component_count = 0;
    #if defined(_WIN32) || defined(_WIN64)
    if (path[0] != '/' && path[0] != '\\' &&
            path[1] == ':' && (path[2] == '/' ||
            path[2] == '\\'))
        i = 2;
    #endif
    while (i < (int)strlen(path)) {
        if (i > 0 && path[i] != '/' &&
                #if defined(_WIN32) || defined(_WIN64)
                path[i] != '\\' &&
                #endif
                (path[i - 1] == '/'
                #if defined(_WIN32) || defined(_WIN64)
                || path[i - 1] == '\\'
                #endif
                )) {
            component_count++;
        }
        i++;
    }
    return component_count;
}

int filesys_IsSymlink(const char *path, int *result) {
    #if defined(_WIN32) || defined(_WIN64)
    *result = 0;
    return 1;
    #else
    struct stat buf;
    int statresult = lstat(path, &buf);
    if (statresult < 0)
        return 0;
    if (result)
        *result = S_ISLNK(buf.st_mode);
    return 1;
    #endif
}

int filesys_RemoveFolder(const char *path, int recursive) {
    #if defined(_WIN32) || defined(_WIN64)
    if (recursive) {
        SHFILEOPSTRUCT shfo = {
            NULL, FO_DELETE, path, NULL,
            FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION,
            FALSE, NULL, NULL
        };
        if (SHFileOperation(&shfo) != 0)
            return 0;
        return (shfo.fAnyOperationsAborted == 0);
    } else {
        return (RemoveDirectoryA(path) != 0);
    }
    #else
    if (recursive) {
        char **contents = NULL;
        int listingworked = filesys_ListFolder(
            path, &contents, 1
        );
        if (!listingworked) {
            assert(contents == NULL);
            return 0;
        }
        int k = 0;
        while (contents[k]) {
            int islink = 0;
            if (!filesys_IsSymlink(contents[k], &islink)) {
                filesys_FreeFolderList(contents);
                return 0;
            }
            if (islink) {
                int result = remove(contents[k]);
                if (result < 0) {
                    filesys_FreeFolderList(contents);
                    return 0;
                }
            } else if (filesys_IsDirectory(contents[k])) {
                if (!filesys_RemoveFolder(contents[k], 1)) {
                    filesys_FreeFolderList(contents);
                    return 0;
                }
            } else {
                if (!filesys_RemoveFile(contents[k])) {
                    filesys_FreeFolderList(contents);
                    return 0;
                }
            }
            k++;
        }
        filesys_FreeFolderList(contents);
        return filesys_RemoveFolder(path, 0);
    } else {
        return (rmdir(path) == 0);
    }
    #endif
}

int filesys_RemoveFile(const char *path) {
    int result = remove(path);
    if (result < 0)
        return 0;
    return 1;
}

char *filesys_RemoveDoubleSlashes(const char *path) {
    if (!path)
        return NULL;
    char *p = strdup(path);
    if (!p)
        return NULL;

    // Remove double slashes:
    int lastwassep = 0;
    int i = 0;
    while (i < (int)strlen(p)) {
        if (p[i] == '/'
                #if defined(_WIN32) || defined(_WIN64)
                || p[i] == '\\'
                #endif
                ) {
            #if defined(_WIN32) || defined(_WIN64)
            p[i] = '\\';
            #endif
            if (!lastwassep) {
                lastwassep = 1;
            } else {
                memmove(
                    p + i, p + i + 1,
                    strlen(p) - i
                );
                continue;
            }
        } else {
            lastwassep = 0;
        }
        i++;
    }
    if (strlen(p) > 1 && (
            p[strlen(p) - 1] == '/'
            #if defined(_WIN32) || defined(_WIN64)
            || p[strlen(p) - 1] == '\\'
            #endif
            )) {
        p[strlen(p) - 1] = '\0';
    }
    return p;
}


char *filesys_Normalize(const char *path) {
    char *result = filesys_RemoveDoubleSlashes(path);
    if (!result)
        return NULL;

    // Remove all unnecessary ../ and ./ inside the path:
    int last_component_start = -1;
    int i = 0;
    while (i < (int)strlen(result)) {
        if ((result[i] == '/'
                #if defined(_WIN32) || defined(_WIN64)
                || result[i] == '\\'
                #endif
                ) && result[i + 1] == '.' &&
                result[i + 2] == '.' && (
                result[i + 3] == '/'
                #if defined(_WIN32) || defined(_WIN64)
                || result[i + 3] == '\\'
                #endif
                || result[i + 3] == '\0'
                ) && i > last_component_start && i > 0 &&
                (result[last_component_start + 1] != '.' ||
                 result[last_component_start + 2] != '.' ||
                 (result[last_component_start + 3] != '/'
                  #if defined(_WIN32) || defined(_WIN64)
                  && result[last_component_start + 3] != '\\'
                  #endif
                 )
                )) {
            // Collapse ../ into previous component:
            int movelen = 4;
            if (result[i + 3] == '\0')
                movelen = 3;
            memmove(result + last_component_start + 1,
                    result + (i + movelen),
                    strlen(result) - (i + movelen) + 1);
            // Start over from beginning:
            i = 0;
            last_component_start = 0;
            continue;
        } else if ((result[i] == '/'
                #if defined(_WIN32) || defined(_WIN64)
                || result[i] == '\\'
                #endif
                ) && result[i + 1] == '.' && (
                result[i + 2] == '/'
                #if defined(_WIN32) || defined(_WIN64)
                || result[i + 2] == '\\'
                #endif
                )) {
            // Collapse unncessary ./ away:
            last_component_start = i;
            memmove(result + i, result + (i + 2),
                    strlen(result) - (i - 2) + 1);
            continue;
        } else if (result[i] == '/'
                #if defined(_WIN32) || defined(_WIN64)
                || result[i] == '\\'
                #endif
                ) {
            last_component_start = i;
            // Collapse all double slashes away:
            while (result[i + 1] == '/'
                    #if defined(_WIN32) || defined(_WIN64)
                    || result[i] == '\\'
                    #endif
                    ) {
                memmove(result + i, result + (i + 1),
                        strlen(result) - (i - 1) + 1);
            }
        }
        i++;
    }

    // Remove leading ./ instances:
    while (strlen(result) >= 2 && result[0] == '.' && (
            result[1] == '/'
            #if defined(_WIN32) || defined(_WIN64)
            || result[1] == '\\'
            #endif
            )) {
        memmove(result, result + 2, strlen(result) + 1 - 2);
    }

    // Unify path separators:
    i = 0;
    while (i < (int)strlen(result)) {
        if (result[i] == '/'
                #if defined(_WIN32) || defined(_WIN64)
                || result[i] == '\\'
                #endif
                ) {
            #if defined(_WIN32) || defined(_WIN64)
            result[i] = '\\';
            #else
            result[i] = '/';
            #endif
        }
        i++;
    }

    // Remove trailing path separators:
    while (strlen(result) > 0) {
        if (result[strlen(result) - 1] == '/'
                #if defined(_WIN32) || defined(_WIN64)
                || result[strlen(result) - 1] == '\\'
                #endif
                ) {
            result[strlen(result) - 1] = '\0';
        } else {
            break;
        }
    }
    return result;
}

int filesys_FileExists(const char *path) {
    #if defined(ANDROID) || defined(__ANDROID__) || defined(__unix__) || defined(__linux__) || defined(__APPLE__) || defined(__OSX__)
    struct stat sb;
    return (stat(path, &sb) == 0);
    #elif defined(_WIN32) || defined(_WIN64)
    DWORD dwAttrib = GetFileAttributes(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES);
    #else
    #error "unsupported platform"
    #endif
}


int filesys_IsDirectory(const char *path) {
    #if defined(ANDROID) || defined(__ANDROID__) || defined(__unix__) || defined(__linux__) || defined(__APPLE__) || defined(__OSX__)
    struct stat sb;
    return (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode));
    #elif defined(_WIN32) || defined(_WIN64)
    DWORD dwAttrib = GetFileAttributes(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
           (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
    #else
    #error "unsupported platform"
    #endif
}


int filesys_GetSize(const char *path, uint64_t *size) {
    #if defined(ANDROID) || defined(__ANDROID__) || defined(__unix__) || defined(__linux__) || defined(__APPLE__) || defined(__OSX__)
    struct stat64 statbuf;

    if (stat64(path, &statbuf) == -1) {
        return 0;
    }
    *size = (uint64_t)statbuf.st_size;
    return 1;
    #else
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesEx(path, GetFileExInfoStandard, &fad))
        return -1;
    LARGE_INTEGER v;
    v.HighPart = fad.nFileSizeHigh;
    v.LowPart = fad.nFileSizeLow;
    *size = (uint64_t)v.QuadPart;
    return 1;
    #endif
}


void filesys_RequestFilesystemAccess() {
    #if defined(ANDROID) || defined(__ANDROID__)
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    jclass javaclass = (*env)->GetObjectClass(env, activity);
    jmethodID method_id = (*env)->GetMethodID(
        env, javaclass, "askFilesystemPermission", "()V"
    );
    (*env)->CallVoidMethod(env, activity, method_id, 0);
    (*env)->DeleteLocalRef(env, activity);
    (*env)->DeleteLocalRef(env, javaclass);
    #endif
}


int filesys_CreateDirectory(const char *path) {
    #if defined(ANDROID) || defined(__ANDROID__) || defined(__unix__) || defined(__linux__) || defined(__APPLE__) || defined(__OSX__)
    return (mkdir(path, 0700) == 0);
    #elif defined(_WIN32) || defined(_WIN64)
    return (CreateDirectory(path, NULL) != 0);
    #else
    #error "unsupported platform"
    #endif
}


static char *_documentspath = NULL;

const char *_filesys_DocumentsBasePath() {
    if (_documentspath)
        return _documentspath;
    #if defined(ANDROID) || defined(__ANDROID__)
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    jclass javaclass = (*env)->GetObjectClass(env, activity);
    jmethodID method_id = (*env)->GetMethodID(
        env, javaclass, "getPublicStorageDir", "()Ljava/lang/String;"
    );
    jstring rv = (*env)->CallObjectMethod(env, activity, method_id, 0);
    char *dirp = strdup((*env)->GetStringUTFChars(env, rv, 0));
    (*env)->DeleteLocalRef(env, rv);
    (*env)->DeleteLocalRef(env, activity);
    (*env)->DeleteLocalRef(env, javaclass);
    if (dirp) {
        _documentspath = malloc(strlen(dirp) +
            strlen("/Documents") + 1);
        if (_documentspath) {
            memcpy(_documentspath, dirp, strlen(dirp));
            memcpy(_documentspath + strlen(dirp),
                   "/Documents",
                   strlen("/Documents") + 1);
        }
        free(dirp);
    } else {
        _documentspath = NULL;
    }
    #else
    #if defined(__unix__) || defined(__linux__) || defined(__APPLE__) || defined(__OSX__)
    const char *dirp = getenv("HOME");
    if (!dirp)
        dirp = getpwuid(getuid())->pw_dir;
    if (dirp) {
        _documentspath = malloc(strlen(dirp) +
            strlen("/Documents") + 1);
        if (_documentspath) {
            memcpy(_documentspath, dirp, strlen(dirp));
            memcpy(_documentspath + strlen(dirp),
                   "/Documents",
                   strlen("/Documents") + 1);
        }
    }
    #elif defined(_WIN32) || defined(_WIN64)
    TCHAR szPath[MAX_PATH + 1];
    if (SUCCEEDED(SHGetFolderPath(NULL,
            CSIDL_MYDOCUMENTS|CSIDL_FLAG_CREATE, NULL, 0, szPath
            )))
        _documentspath = strdup(szPath);
    #else
    #error "unsupported platform"
    #endif
    #endif
    if (_documentspath && !filesys_IsDirectory(_documentspath)) {
        filesys_CreateDirectory(_documentspath);
    }
    return _documentspath;
}


static char *_appdatapath = NULL;
const char *filesys_AppDataSubFolder(const char *appname) {
    if (_appdatapath)
        return _appdatapath;
    char emptybuf[] = "";
    if (!appname)
        appname = emptybuf;
    #if defined(ANDROID) || defined(__ANDROID__)
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    jclass javaclass = (*env)->GetObjectClass(env, activity);
    jmethodID method_id = (*env)->GetMethodID(
        env, javaclass, "getAppDataDir", "()Ljava/lang/String;"
    );
    jstring rv = (*env)->CallObjectMethod(env, activity, method_id, 0);
    _appdatapath = strdup((*env)->GetStringUTFChars(env, rv, 0));
    (*env)->DeleteLocalRef(env, rv);
    (*env)->DeleteLocalRef(env, activity);
    (*env)->DeleteLocalRef(env, javaclass);
    #else
    #if defined(__APPLE__) || defined(__OSX__)
    const char *dirp = getenv("HOME");
    if (!dirp)
        dirp = getpwuid(getuid())->pw_dir;
    if (dirp) {
        _appdatapath = malloc(
            strlen(dirp) +
            strlen(LINUX_APPDATA_SUFFIX) + 1 +
            strlen(appname) + 1
        );
        if (_appdatapath) {
            memcpy(_appdatapath, dirp, strlen(dirp));
            memcpy(_appdatapath + strlen(dirp),
                   MACOS_APPDATA_SUFFIX,
                   strlen(MACOS_APPDATA_SUFFIX) + 1);
            memcpy(
                _appdatapath + strlen(dirp) +
                strlen(MACOS_APPDATA_SUFFIX) + 1,
                appname, strlen(appname) + 1
            );
        }
    }
    #elif defined(__unix__) || defined(__linux__)
    const char *dirp = getenv("HOME");
    if (!dirp)
        dirp = getpwuid(getuid())->pw_dir;
    if (dirp) {
        _appdatapath = malloc(
            strlen(dirp) +
            strlen(LINUX_APPDATA_SUFFIX) + 1 +
            strlen(appname) + 1
        );
        if (_appdatapath) {
            memcpy(_appdatapath, dirp, strlen(dirp));
            memcpy(_appdatapath + strlen(dirp),
                   LINUX_APPDATA_SUFFIX,
                   strlen(LINUX_APPDATA_SUFFIX) + 1);
            memcpy(
                _appdatapath + strlen(dirp) +
                strlen(LINUX_APPDATA_SUFFIX) + 1,
                appname, strlen(appname) + 1
            );
        }
    }
    #elif defined(_WIN32) || defined(_WIN64)
    TCHAR path[MAX_PATH+1];
    if (SUCCEEDED(SHGetFolderPath(NULL,
            CSIDL_APPDATA|CSIDL_FLAG_CREATE, NULL, 0, path
            ))) {
        path[MAX_PATH] = '\0';
        _appdatapath = malloc(
            strlen(path) +
            strlen(WINDOWS_APPDATA_SUFFIX) + 1 +
            strlen(appname) + 1
        );
        if (_appdatapath) {
            memcpy(_appdatapath, path, strlen(path));
            memcpy(_appdatapath + strlen(path),
                   WINDOWS_APPDATA_SUFFIX,
                   strlen(WINDOWS_APPDATA_SUFFIX) + 1);
            memcpy(
                _appdatapath + strlen(path) +
                strlen(WINDOWS_APPDATA_SUFFIX) + 1,
                appname, strlen(appname) + 1
            );
        }
    }
    #else
    #error "unsupported platform"
    #endif
    #endif
    if (_appdatapath && !filesys_IsDirectory(_appdatapath)) {
        filesys_CreateDirectory(_appdatapath);
    }
    return _appdatapath;
}


static char docsubfolderbuf[4096];

const char *filesys_DocumentsSubFolder(
        const char *subfolder
        ) {
    const char *docsfolder = _filesys_DocumentsBasePath();
    if (!docsfolder)
        return NULL;

    if (!subfolder || strlen(subfolder) == 0)
        return docsfolder;

    snprintf(
        docsubfolderbuf, sizeof(docsubfolderbuf) - 1,
        "%s%s%s%s",
        docsfolder, (
        #if defined(_WIN32) || defined(_WIN64)
        "\\"
        #else
        "/"
        #endif
        ),
        subfolder, (
        #if defined(_WIN32) || defined(_WIN64)
        "\\"
        #else
        "/"
        #endif
        )
    );
        
    if (!filesys_IsDirectory(docsubfolderbuf)) {
        filesys_CreateDirectory(docsubfolderbuf);
    }

    return docsubfolderbuf;
}


void filesys_FreeFolderList(char **list) {
    int i = 0;
    while (list[i]) {
        free(list[i]);
        i++;
    }
    free(list);
}


int filesys_ListFolder(const char *path,
                       char ***contents,
                       int returnFullPath) {
    #if defined(_WIN32) || defined(_WIN64)
    WIN32_FIND_DATA ffd;
    int isfirst = 1;
    char *p = malloc(strlen(path) + 3);
    if (!p)
        return 0;
    memcpy(p, path, strlen(path) + 1);
    if (p[strlen(p) - 1] != '\\') {
        p[strlen(p) + 1] = '\0';
        p[strlen(p)] = '\\';
    }
    p[strlen(p) + 1] = '\0';
    p[strlen(p)] = '*';
    HANDLE hFind = FindFirstFile(p, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        free(p);
        return 0;
    }
    free(p);
    #else
    DIR *d = opendir(path);
    if (!d)
        return 0;
    #endif
    char **list = malloc(sizeof(char*));
    list[0] = NULL;
    char **fullPathList = NULL;
    int entriesSoFar = 0;
    while (1) {
        #if defined(_WIN32) || defined(_WIN64)
        if (isfirst) {
            isfirst = 0;
        } else {
            if (FindNextFile(hFind, &ffd) == 0) {
                if (GetLastError() != ERROR_NO_MORE_FILES)
                    goto errorquit;
                break;
            }
        }
        const char *entryName = ffd.cFileName;
        #else
        errno = 0;
        struct dirent *entry = readdir(d);
        if (!entry && errno != 0) {
            goto errorquit;
        }
        if (!entry)
            break;
        const char *entryName = entry->d_name;
        if (strcmp(entryName, ".") == 0 || strcmp(entryName, "..") == 0) {
            continue;
        }
        #endif
        char **nlist = realloc(list, sizeof(char*) * (entriesSoFar + 2));
        if (!nlist) {
            errorquit:
            #if defined(_WIN32) || defined(_WIN64)
            if (hFind != INVALID_HANDLE_VALUE)
                FindClose(hFind);
            #else
            if (d)
                closedir(d);
            #endif
            if (list) {
                int k = 0;
                while (k < entriesSoFar) {
                    if (list[k])
                        free(list[k]);
                    else
                        break;
                    k++;
                }
                free(list);
            }
            if (fullPathList) {
                int k = 0;
                while (fullPathList[k]) {
                    free(fullPathList[k]);
                    k++;
                }
                free(fullPathList);
            }
            return 0;
        }
        list = nlist;
        entriesSoFar++;
        list[entriesSoFar] = NULL;
        list[entriesSoFar - 1] = strdup(entryName);
        if (!list[entriesSoFar - 1])
            goto errorquit;
    }
    #if defined(_WIN32) || defined(_WIN64)
    FindClose(hFind);
    hFind = INVALID_HANDLE_VALUE;
    #else
    closedir(d);
    d = NULL;
    #endif
    if (!returnFullPath) {
        *contents = list;
    } else {
        fullPathList = malloc(sizeof(char*) * (entriesSoFar + 1));
        if (!fullPathList)
            goto errorquit;
        int k = 0;
        while (k < entriesSoFar) {
            fullPathList[k] = malloc(strlen(path) + 1 + strlen(list[k]) + 1);
            if (!fullPathList[k])
                goto errorquit;
            memcpy(fullPathList[k], path, strlen(path));
            #if defined(_WIN32) || defined(_WIN64)
            fullPathList[k][strlen(path)] = '\\';
            #else
            fullPathList[k][strlen(path)] = '/';
            #endif
            memcpy(fullPathList[k] + strlen(path) + 1, list[k], strlen(list[k]) + 1);
            k++;
        }
        fullPathList[entriesSoFar] = NULL;
        k = 0;
        while (k < entriesSoFar && list[k]) {
            free(list[k]);
            k++;
        }
        free(list);
        *contents = fullPathList;
    }
    return 1;
}


char *filesys_GetRealPath(const char *s) {
    #if defined(_WIN32) || defined(_WIN64)
    return strdup(s);
    #else
    if (!s)
        return NULL;
    return realpath(s, NULL);
    #endif
}


char *filesys_GetOwnExecutable() {
    #if defined(_WIN32) || defined(_WIN64)
    TCHAR fp[MAX_PATH + 1];
    GetModuleFileName(NULL, fp, MAX_PATH + 1);
    fp[MAX_PATH] = '\0';
    return strdup(fp);
    #else
    int alloc = 16;
    char *fpath = malloc(alloc);
    if (!fpath)
        return NULL;
    while (1) {
        fpath[0] = '\0';
        #if defined(APPLE) || defined(__APPLE__)
        int i = alloc;
        if (_NSGetExecutablePath(fpath, &i) != 0) {
            free(fpath);
            fpath = malloc(i + 1);
            if (_NSGetExecutablePath(fpath, &i) != 0) {
                free(fpath);
                return NULL;
            }
            return fpath;
        }     
        #else
        int written = readlink("/proc/self/exe", fpath, alloc);
        if (written >= alloc) {
            alloc *= 2;
            free(fpath);
            fpath = malloc(alloc);
            if (!fpath)
                return NULL;
            continue;
        } else if (written <= 0) {
            free(fpath);
            return NULL;
        }
        fpath[written] = '\0';
        return fpath;
        #endif
    }
    #endif
}

char *filesys_Dirname(const char *path) {
    if (!path)
        return NULL;
    char *s = strdup(path);
    if (!s)
        return NULL;
    int cutoffdone = 0;
    int evernotslash = 0;
    int i = strlen(s) - 1;
    while (i >= 0) {
        if (evernotslash && (s[i] == '/'
                #if defined(_WIN32) || defined(_WIN64)
                || s[i] == '\\'
                #endif
                )) {
            s[i] = '\0';
            i--;
            while (i >= 0 &&
                    (s[i] == '/'
                    #if defined(_WIN32) || defined(_WIN64)
                    || s[i] == '\\'
                    #endif
                    )) {
                s[i] = '\0';
                i--;
            }
            cutoffdone = 1;
            break;
        } else if (s[i] != '/'
                #if defined(_WIN32) || defined(_WIN64)
                && s[i] != '\\'
                #endif
                ) {
            evernotslash = 1;
        }
        i--;
    }
    if (!cutoffdone) s[0] = '\0';
    return s;
}

char *filesys_Basename(const char *path) {
    int i = 0;
    while ((int)strlen(path) > i &&
            path[strlen(path) - i - 1] != '/'
            #if defined(_WIN32) || defined(_WIN64)
            &&
            path[strlen(path) - i - 1] != '\\'
            #endif
            )
        i++;
    char *result = malloc(i + 1);
    if (!result)
        return result;
    memcpy(result, path + strlen(path) - i, i + 1);
    return result;
}


char *filesys_ParentdirOfItem(const char *path) {
    if (!path)
        return NULL;
    char *p = strdup(path);
    if (!p)
        return NULL;

    // If this is already shortened to absolute path root, abort:
    #if defined(_WIN32) || defined(_WIN64)
    if (strlen(path) >= 2 && strlen(path) <= 3 &&
            path[1] == ':' && (strlen(path) == 2 ||
            path[2] == '/' || path[2] == '\\') &&
            ((path[0] >= 'a' && path[0] <= 'z') ||
              path[0] >= 'A' && path[0] <= 'Z'))
        return p;
    #else
    if (strlen(path) == 1 && path[0] == '/')
        return p;
    #endif

    // Strip trailing slash if any, then go back one component:
    #if defined(_WIN32) || defined(_WIN64)
    while (strlen(p) > 0 && (
            p[strlen(p) - 1] == '/' ||
            p[strlen(p) - 1] == '\\'))
        p[strlen(p) - 1] = '\0';
    while (strlen(p) > 0 &&
            p[strlen(p) - 1] != '/' &&
            p[strlen(p) - 1] != '\\')
        p[strlen(p) - 1] = '\0';
    #else
    while (strlen(p) > 0 &&
            p[strlen(p) - 1] == '/')
        p[strlen(p) - 1] = '\0';
    while (strlen(p) > 0 &&
            p[strlen(p) - 1] != '/')
        p[strlen(p) - 1] = '\0';
    #endif
    return p;
}


char *filesys_Join(const char *path1, const char *path2_orig) {
    // Quick result paths:
    if (!path1 || !path2_orig)
        return NULL;
    if (strcmp(path2_orig, ".") == 0 || strcmp(path2_orig, "") == 0)
        return strdup(path1);

    // Clean up path2 for merging:
    char *path2 = strdup(path2_orig);
    if (!path2)
        return NULL;
    while (strlen(path2) >= 2 && path2[0] == '.' &&
            (path2[1] == '/'
            #if defined(_WIN32) || defined(_WIN64)
            || path2[1] == '\\'
            #endif
            )) {
        memmove(path2, path2 + 2, strlen(path2) + 1 - 2);
        if (strcmp(path2, "") == 0 || strcmp(path2, ".") == 0) {
            free(path2);
            return strdup(path1);
        }
    }

    // Do actual merging:
    char *presult = malloc(strlen(path1) + 1 + strlen(path2) + 1);
    if (!presult) {
        free(path2);
        return NULL;
    }
    memcpy(presult, path1, strlen(path1) + 1);
    presult[strlen(path1)] = '\0';
    if (strlen(path1) > 0) {
        presult[strlen(path1) + 1] = '\0';
        #if defined(_WIN32) || defined(_WIN64)
        if (path1[strlen(path1) - 1] != '\\' &&
                path1[strlen(path1) - 1] != '/' &&
                (strlen(path2) == 0 || path2[0] != '\\' ||
                 path2[0] != '/'))
            presult[strlen(path1)] = '\\';
        #else
        if ((path1[strlen(path1) - 1] != '/') &&
                (strlen(path2) == 0 || path2[0] != '/'))
            presult[strlen(path1)] = '/';
        #endif
        memcpy(presult + strlen(presult), path2, strlen(path2) + 1);
    } else {
        #if defined(_WIN32) || defined(_WIN64)
        if (strlen(path2) == 0 ||
                path2[0] == '/' ||
                path2[0] == '\\')
            memcpy(presult + strlen(presult),
                   path2 + 1, strlen(path2));
        else
            memcpy(presult + strlen(presult), path2,
                   strlen(path2) + 1);
        #else
        if (strlen(path2) == 0 ||
                path2[0] == '/')
            memcpy(presult + strlen(presult),
                   path2 + 1, strlen(path2));
        else
            memcpy(presult + strlen(presult), path2,
                   strlen(path2) + 1);
        #endif
    }
    free(path2);
    return presult;
}


int filesys_LaunchExecutable(const char *path, int argcount, ...) {
    va_list args;
    va_start(args, argcount);
    int argc = 0;
    char **argv = malloc(sizeof(*argv) * 2);
    if (argv) {
        argv[0] = strdup(path);
        argv[1] = NULL;
        if (argv[0]) {
            argc++;
        } else {
            free(argv);
            argv = NULL;
        }
    }
    while (argcount > 0) {
        argcount--;
        char *val = va_arg(args, char*);
        if (!val || !argv)
            continue;
        char **newargv = malloc(sizeof(*argv) * (argc + 2));
        int i = 0;
        if (newargv) {
            i = 0;
            while (i < argc) {
                newargv[i] = argv[i];
                i++;
            }
            free(argv);
            argv = newargv;
            argv[argc] = strdup(val);
            if (argv[argc]) {
                argv[argc + 1] = NULL;
                argc++;
            } else {
                goto dumpargv;
            }
        } else {
            dumpargv:
            i = 0;
            while (i < argc) {
                free(argv[i]);
                i++;
            }
            free(argv);
            argv = NULL;
        }
    }
    va_end(args);
    if (!argv) 
        return 0;
    int success = 1;
    #if defined(_WIN32) || defined(_WIN64)
    char *cmd = strdup("");
    if (!cmd)
        return 0;
    int i = 0;
    while (i < argc) {
        char *newcmd = realloc(
            cmd, strlen(cmd) + 3 + strlen(argv[i]) * 2 + 1
        );
        if (!newcmd) {
            success = 0;
            goto ending;
        }
        cmd = newcmd;
        if (i > 0) {
            cmd[strlen(cmd) + 1] = '\0';
            cmd[strlen(cmd)] = ' ';
        }
        cmd[strlen(cmd) + 1] = '\0';
        cmd[strlen(cmd)] = '\"';
        int k = 0;
        while (k < (int)strlen(argv[i])) {
            char c = argv[i][k];
            if (c == '"' || c == '\\') {
                cmd[strlen(cmd) + 1] = '\0';
                cmd[strlen(cmd)] = '\\';
            }
            cmd[strlen(cmd) + 1] = '\0';
            cmd[strlen(cmd)] = c;
            k++;
        }
        cmd[strlen(cmd) + 1] = '\0';
        cmd[strlen(cmd)] = '\"';
        i++;
    }
    STARTUPINFO info;
    memset(&info, 0, sizeof(info));
    PROCESS_INFORMATION pinfo;
    memset(&pinfo, 0, sizeof(pinfo));
    success = 0;
    if (CreateProcess(path, cmd, NULL, NULL, TRUE, 0, NULL,
                      NULL, &info, &pinfo))
    {
        success = 1;
        CloseHandle(pinfo.hProcess);
        CloseHandle(pinfo.hThread);
    }
    #else
    if (fork() == 0) {
        execvp(path, argv);
        // This should never be reached:
        exit(1);
        return 0;
    }
    #endif
    #if defined(_WIN32) || defined(_WIN64)
    ending: ;
    #endif
    int j = 0;
    while (j < argc) {
        free(argv[j]);
        j++;
    }
    free(argv);
    argv = NULL;
    return success;
}

int filesys_IsAbsolutePath(const char *path) {
    if (strlen(path) > 0 && path[0] == '.')
        return 0;
    #if (!defined(_WIN32) && !defined(_WIN64))
    if (strlen(path) > 0 && path[0] == '/')
        return 1;
    #endif
    #if defined(_WIN32) || defined(_WIN64)
    if (strlen(path) > 2 && (
            path[1] == ':' || path[1] == '\\'))
        return 1;
    #endif
    return 0;
}

char *filesys_ToAbsolutePath(const char *path) {
    if (filesys_IsAbsolutePath(path))
        return strdup(path);
    char *cwd = filesys_GetCurrentDirectory();
    if (!cwd)
        return NULL;
    char *result = filesys_Join(cwd, path);
    free(cwd);
    return result;
}

int filesys_PathCompare(const char *p1, const char *p2) {
    char *p1normalized = filesys_Normalize(p1);
    char *p2normalized = filesys_Normalize(p2);
    if (!p1normalized || !p2normalized) {
        if (p1normalized)
            free(p1normalized);
        if (p2normalized)
            free(p2normalized);
        return -1;
    }
    int result = strcmp(p1normalized, p2normalized) == 0;
    free(p1normalized);
    free(p2normalized);
    return result;
}

char *filesys_GetCurrentDirectory() {
    #if defined(_WIN32) || defined(_WIN64)
    DWORD size = GetCurrentDirectory(0, NULL);
    char *s = malloc(size + 1);
    if (!s)
        return NULL;
    if (GetCurrentDirectory(size, s) != 0) {
        s[size - 1] = '\0';
        return s;
    }
    free(s);
    return NULL;
    #else
    int allocsize = 32;
    while (1) {
        allocsize *= 2;
        char *s = malloc(allocsize);
        if (!s)
            return NULL;
        char *result = getcwd(s, allocsize - 1);
        if (result == NULL) {
            free(s);
            if (errno == ERANGE) {
                continue;
            }
            return NULL;
        }
        s[allocsize - 1] = '\0';
        return s;
    }
    #endif
}

char *filesys_TurnIntoPathRelativeTo(
        const char *path, const char *makerelativetopath
        ) {
    if (!path)
        return NULL;
    char *cwd = filesys_GetCurrentDirectory();
    if (!cwd)
        return NULL;

    // Prepare input to be absolute & normalized:
    char *input_path = NULL;
    if (filesys_IsAbsolutePath(path)) {
        input_path = strdup(path);
    } else {
        input_path = filesys_Join(cwd, path);
    }
    if (!input_path) {
        free(cwd);
        return NULL;
    }
    {
        char *_s = filesys_Normalize(input_path);
        if (!_s) {
            free(input_path);
            free(cwd);
            return NULL;
        }
        free(input_path);
        input_path = _s;
    }

    // Prepare comparison path to be absolute & normalized:
    char *reltopath = NULL;
    if (makerelativetopath &&
            filesys_IsAbsolutePath(makerelativetopath)) {
        reltopath = strdup(makerelativetopath);
    } else if (makerelativetopath) {
        reltopath = filesys_Join(cwd, makerelativetopath);
    } else {
        reltopath = strdup(cwd);
    }
    if (!reltopath) {
        free(input_path);
        free(cwd);
        return NULL;
    }
    {
        char *_s = filesys_Normalize(reltopath);
        if (!_s) {
            free(reltopath);
            free(input_path);
            free(cwd);
            return NULL;
        }
        free(reltopath);
        reltopath = _s;
    }

    // Free unneeded resources:
    free(cwd);
    cwd = NULL;

    // Get the similar path base:
    int similar_up_to = -1;
    int last_component = -1;
    int i = 0;
    while (i < (int)strlen(reltopath) && i < (int)strlen(input_path)) {
        if (reltopath[i] == input_path[i]) {
            similar_up_to = i;
        } else {
            break;
        }
        if (reltopath[i] == '/'
                #if defined(_WIN32) || defined(_WIN64)
                || reltopath[i] == '\\'
                #endif
                ) {
            last_component = i;
        }
        i++;
    }
    if (similar_up_to + 1 >= (int)strlen(reltopath) &&
            (similar_up_to + 1 < (int)strlen(input_path) && (
            input_path[similar_up_to + 1] == '/'
            #if defined(_WIN32) || defined(_WIN64)
            || input_path[similar_up_to + 1] == '\\'
            #endif
            ))) {
        last_component = similar_up_to + 1;
    } else if (similar_up_to + 1 >= (int)strlen(input_path) &&
            (similar_up_to + 1 < (int)strlen(reltopath) && (
            reltopath[similar_up_to + 1] == '/'
            #if defined(_WIN32) || defined(_WIN64)
            || reltopath[similar_up_to + 1] == '\\'
            #endif
            ))) {
        last_component = similar_up_to + 1;
    }
    if (similar_up_to > last_component)
        similar_up_to = last_component;

    char *samestart = strdup(input_path);
    if (!samestart) {
        free(input_path);
        free(reltopath);
        return NULL;
    }
    samestart[similar_up_to + 1] = '\0';
    {
        char *_s = filesys_Normalize(samestart);
        free(samestart);
        samestart = NULL;
        if (!_s) {
            free(reltopath);
            return NULL;
        }
        samestart = _s;
    }

    char *differingend = strdup(input_path);
    free(input_path);
    input_path = NULL;
    if (!differingend) {
        free(reltopath);
        free(samestart);
        return NULL;
    }

    if (similar_up_to > 0) {
        memmove(
            differingend, differingend + (similar_up_to + 1),
            strlen(differingend) - (similar_up_to + 1) + 1
        );
    }
    while (strlen(differingend) > 0 && (
            differingend[0] == '/'
            #if defined(_WIN32) || defined(_WIN64)
            || differingend[0] == '\\'
            #endif
            )) {
        memmove(differingend, differingend + 1, strlen(differingend));
    }

    int samestart_components = filesys_GetComponentCount(samestart);
    int reltopath_components = filesys_GetComponentCount(reltopath);

    free(reltopath);
    reltopath = NULL;
    free(samestart);
    samestart = NULL;

    i = samestart_components;
    while (i < reltopath_components) {
        char *_s = malloc(strlen(differingend) + strlen("../") + 1);
        if (!_s) {
            free(differingend);
            return NULL;
        }
        memcpy(
            _s,
            #if defined(_WIN32) || defined(_WIN64)
            "..\\",
            #else
            "../",
            #endif
            strlen("../")
        );
        memcpy(_s + strlen("../"), differingend, strlen(differingend) + 1);
        free(differingend);
        differingend = _s;
        i++;
    }

    return differingend;
}


// A few brief unit tests:
__attribute__((constructor)) static void _tests() {
    char *n = filesys_Normalize("u//abc/def/..u/../..");
    if (n) {
        assert(strcmp(n, "u/abc") == 0 ||
               strcmp(n, "u\\abc") == 0);
        free(n);
    }
    n = filesys_Normalize("u//../abc/def/..u/../..");
    if (n) {
        assert(strcmp(n, "abc") == 0);
        free(n);
    }
    n = filesys_Normalize("../abc/def/..u/../..");
    if (n) {
        assert(strcmp(n, "../abc") == 0 ||
               strcmp(n, "..\\abc") == 0);
        free(n);
    }
    n = filesys_TurnIntoPathRelativeTo(
        "/abc/def/lul", "/abc//def/flobb/"
    );
    if (n) {
        assert(strcmp(n, "../lul") == 0 ||
               strcmp(n, "..\\lul") == 0);
        free(n);
    }
    #if defined(_WIN32) || defined(_WIN64)
    n = filesys_TurnIntoPathRelativeTo(
        "C:/home/ellie/Develop//game-newhorror/levels/../textures/outdoors/sand.png",
        "C:/home/../home/ellie/Develop/game-newhorror"
    );
    #else
    n = filesys_TurnIntoPathRelativeTo(
        "/home/ellie/Develop//game-newhorror/levels/../textures/outdoors/sand.png",
        "/home/../home/ellie/Develop/game-newhorror"
    );
    #endif
    if (n) {
        assert(strcmp(n, "textures/outdoors/sand.png") == 0 ||
               strcmp(n, "textures\\outdoors\\sand.png") == 0);
        free(n);
    }
    #if defined(_WIN32) || defined(_WIN64)
    n = filesys_TurnIntoPathRelativeTo(
        "C:/home/ellie/Develop/game-newhorror/levels/textures/"
        "misc/notexture_NOCOLLISION_INVISIBLE.png",
        "C:/home/ellie/Develop/game-newhorror/"
    );
    #else
    n = filesys_TurnIntoPathRelativeTo(
        "/home/ellie/Develop/game-newhorror/levels/textures/"
        "misc/notexture_NOCOLLISION_INVISIBLE.png",
        "/home/ellie/Develop/game-newhorror/"
    );
    #endif
    if (n) {
        assert(
            strcmp(n, "levels/textures/misc/"
                      "notexture_NOCOLLISION_INVISIBLE.png") == 0 ||
            strcmp(n, "levels\\textures\\misc\\"
                      "notexture_NOCOLLISION_INVISIBLE.png") == 0
        );
        free(n);
    }
}

int filesys_FolderContainsPath(
        const char *folder_path, const char *check_path,
        int *result
        ) {
    if (!folder_path || !check_path)
        return 0;
    char *fnormalized = filesys_Normalize(folder_path);
    char *checknormalized = filesys_Normalize(check_path);
    if (!fnormalized || !checknormalized) {
        free(fnormalized);
        free(checknormalized);
        return 0;
    }
    if (strlen(fnormalized) < strlen(checknormalized) && (
            checknormalized[strlen(fnormalized)] == '/'
            #if defined(_WIN32) || defined(_WIN64)
            || checknormalized[strlen(fnormalized)] == '\\'
            #endif
            )) {
        free(fnormalized);
        free(checknormalized);
        *result = 1;
        return 1;
    }
    free(fnormalized);
    free(checknormalized);
    *result = 0;
    return 1;
}
