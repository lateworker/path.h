#include <iostream>
#include <sys/stat.h>
#include <vector>
#include <queue>
#include <io.h>
using namespace std;
struct FileAttibute {
	signed attrib;
	inline void operator= (const unsigned &o) {
		attrib = o;
	}
	inline bool is_normal() {
		return attrib & _A_NORMAL;
	}
	inline bool is_read_only() {
		return attrib & _A_RDONLY;
	}
	inline bool is_hidden() {
		return attrib & _A_HIDDEN;
	}
	inline bool is_system() {
		return attrib & _A_SYSTEM;
	}
	inline bool is_subdir() {
		return attrib & _A_SUBDIR;
	}
	inline bool is_archive() {
		return attrib & _A_ARCH;
	}
};
struct TraverseData {
	FileAttibute attrib;
	unsigned long size;
	string path, name;
	inline string fullpath() {
		return path + "\\" + name;
	}
};
void deleteExtraneousSymbols(string &data) {
	if (data[0] == '\"') data.erase(data.begin());
	if (data[data.length() - 1] == '\"') data.erase(data.end() - 1);
	for (size_t i = data.length() - 1; data[i] == '\\' or data[i] == '/'; --i) {
		data.erase(i);
	}
}
struct TraverseMode {
	bool forFile, forDir, IntoSubdir, includingEmptyDir;
	bool operator== (const TraverseMode &o) const {
		return forFile == o.forFile and forDir == o.forDir and IntoSubdir == o.IntoSubdir;
	}
};
#define TraverseForAll {true, true, true, true}
queue<TraverseData> traverse_temp;

// 参数传递版本
bool _traverse(const string &path, vector<TraverseData> &results, const string &selector, const TraverseMode &mode);
bool _traverseFile(const string &path, vector<TraverseData> &results, const string &selector, const TraverseMode &mode) {
	_finddata_t fileInfo;
	string currentPath = path + "\\" + selector;
	intptr_t handle = _findfirst(currentPath.c_str(), &fileInfo);
	if (handle == -1) return false;
	FileAttibute newAttrib;
	TraverseData fileData, dirData;
	do {
		newAttrib = fileInfo.attrib;
		if (newAttrib.is_subdir()) continue;
		fileData = {
			newAttrib,
			fileInfo.size,
			path, fileInfo.name
		};
		if (!mode.includingEmptyDir) {
			while (!traverse_temp.empty()) {
				dirData = traverse_temp.front();
				traverse_temp.pop();
				if (path.find(dirData.path + "\\" + dirData.name) != string::npos)
					results.push_back(dirData);
			}
			if (!mode.forFile) return false;
		}
		if (mode.forFile) {
			results.push_back(fileData);
		}
	} while (!_findnext(handle, &fileInfo));
	return true;
}
bool _traverseDir(const string &path, vector<TraverseData> &results, const string &selector, const TraverseMode &mode) {
	_finddata_t fileInfo;
	string currentPath = path + "\\*.*";
	intptr_t handle = _findfirst(currentPath.c_str(), &fileInfo);
	if (handle == -1) return false;
	bool returnValue = false;
	string newpath;
	FileAttibute newAttrib;
	TraverseData dirData;
	do {
		newAttrib = fileInfo.attrib;
		if (!newAttrib.is_subdir() or strcmp(fileInfo.name, "..") == 0 or strcmp(fileInfo.name, ".") == 0)
			continue;
		dirData = {
			newAttrib,
			fileInfo.size,
			path, fileInfo.name
		};
		if (mode.forDir) {
			if (mode.includingEmptyDir) {
				results.push_back(dirData);
			} else {
				traverse_temp.push(dirData);
			}
		}
		newpath = path + "\\" + fileInfo.name;
		if (mode.IntoSubdir) {
			if (_traverse(newpath, results, selector, mode))
				returnValue = true;
		} else returnValue = true;
	} while (!_findnext(handle, &fileInfo));
	return returnValue;
}
bool _traverse(const string &path, vector<TraverseData> &results, const string &selector, const TraverseMode &mode) {
	bool returnValue_traverseFile;
	bool returnValue_traverseDir;
	if (mode.forFile or (!mode.includingEmptyDir and !traverse_temp.empty()))
		returnValue_traverseFile = _traverseFile(path, results, selector, mode);
	if (mode.forDir or mode.IntoSubdir)
		returnValue_traverseDir = _traverseDir(path, results, selector, mode);
	return returnValue_traverseFile or returnValue_traverseDir;
}
bool traverse(string path, vector <TraverseData> &results, string selector = "*.*", TraverseMode mode = TraverseForAll) {
	deleteExtraneousSymbols(path);
	return _traverse(path, results, selector, mode);
}

// 回调函数版本
typedef void (*callback)(TraverseData data);
bool _traverseC(const string &path, const callback &fileFunc, const callback &dirFunc, const string &selector, const TraverseMode &mode);
bool _traverseFileC(const string &path, const callback &fileFunc, const callback &dirFunc, const string &selector, const TraverseMode &mode) {
	_finddata_t fileInfo;
	string currentPath = path + "\\" + selector;
	intptr_t handle = _findfirst(currentPath.c_str(), &fileInfo);
	if (handle == -1) return false;
	FileAttibute newAttrib;
	TraverseData fileData, dirData;
	do {
		newAttrib = fileInfo.attrib;
		if (newAttrib.is_subdir()) continue;
		fileData = {
			newAttrib,
			fileInfo.size,
			path, fileInfo.name
		};
		if (!mode.includingEmptyDir) {
			while (!traverse_temp.empty()) {
				dirData = traverse_temp.front();
				traverse_temp.pop();
				if (path.find(dirData.path + "\\" + dirData.name) != string::npos)
					if (dirFunc != nullptr)
						dirFunc(dirData);
			}
			if (!mode.forFile) return false;
		}
		if (mode.forFile) {
			if (fileFunc != nullptr)
				fileFunc(fileData);
		}
	} while (!_findnext(handle, &fileInfo));
	return true;
}
bool _traverseDirC(const string &path, const callback &fileFunc, const callback &dirFunc, const string &selector, const TraverseMode &mode) {
	_finddata_t fileInfo;
	string currentPath = path + "\\*.*";
	intptr_t handle = _findfirst(currentPath.c_str(), &fileInfo);
	if (handle == -1) return false;
	bool returnValue = false;
	string newpath;
	FileAttibute newAttrib;
	TraverseData dirData;
	do {
		newAttrib = fileInfo.attrib;
		if (!newAttrib.is_subdir() or strcmp(fileInfo.name, "..") == 0 or strcmp(fileInfo.name, ".") == 0)
			continue;
		dirData = {
			newAttrib,
			fileInfo.size,
			path, fileInfo.name
		};
		if (mode.forDir) {
			if (mode.includingEmptyDir) {
				if (dirFunc != nullptr)
					dirFunc(dirData);
			} else {
				traverse_temp.push(dirData);
			}
		}
		newpath = path + "\\" + fileInfo.name;
		if (mode.IntoSubdir) {
			if (_traverseC(newpath, fileFunc, dirFunc, selector, mode))
				returnValue = true;
		} else returnValue = true;
	} while (!_findnext(handle, &fileInfo));
	return returnValue;
}
bool _traverseC(const string &path, const callback &fileFunc, const callback &dirFunc, const string &selector, const TraverseMode &mode) {
	bool returnValue_traverseFile;
	bool returnValue_traverseDir;
	if (mode.forFile or (!mode.includingEmptyDir and !traverse_temp.empty()))
		returnValue_traverseFile = _traverseFileC(path, fileFunc, dirFunc, selector, mode);
	if (mode.forDir or mode.IntoSubdir)
		returnValue_traverseDir = _traverseDirC(path, fileFunc, dirFunc, selector, mode);
	return returnValue_traverseFile or returnValue_traverseDir;
}
bool traverseC(string path, callback fileFunc = nullptr, callback dirFunc = nullptr, string selector = "*.*", TraverseMode mode = TraverseForAll) {
	deleteExtraneousSymbols(path);
	return _traverseC(path, fileFunc, dirFunc, selector, mode);
}

struct stat exist_buffer_temp;
inline bool exist(string path, struct stat *buffer = nullptr) {
	if (buffer == nullptr) buffer = &exist_buffer_temp;
	return !stat(path.c_str(), buffer);
}
struct Time {
	long time_access, time_modify, time_create;
	inline void operator= (const struct stat &o) {
		time_access = o.st_atime;
		time_modify = o.st_mtime;
		time_create = o.st_ctime;
	}
};
bool getTimeData(string path, Time &timeData) {
	if (!exist(path)) return true;
	timeData = exist_buffer_temp;
	return false;
}
inline string getFullPath(const string &path, const string &name) {
	return path + "\\" + name;
}
