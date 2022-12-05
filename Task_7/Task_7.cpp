#define _CRT_SECURE_NO_WARNINGS

#include <ctime>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <Windows.h>

using namespace std;

mutex mtx;
mutex mtx2;

// проверка на существование
bool TestDirForPath(HANDLE hf, wstring path) {
    if (hf == nullptr || hf == INVALID_HANDLE_VALUE) {
        mtx.lock();
        wcout << L"Не удалось открыть файл по пути: " << path << endl;
        mtx.unlock();
        return true;
    }
    return false;
}

//сканирование директории
void SearchForDir(wstring path, wstring name, queue<wstring>& pathRezuld) {
    WIN32_FIND_DATA findData;
    queue<thread*> qTreadsLocal;
    HANDLE hf = FindFirstFile((path + L"*").c_str(), &findData);
    if (TestDirForPath(hf, path)) return;
    do {
        try {
            if (TestDirForPath(hf, path)) continue;
            // пропускаем директории "." и ".."
            if (!wcsncmp(findData.cFileName, L".", 1) ||
                !wcsncmp(findData.cFileName, L"..", 2))
                continue;
            wstring filePath = path + findData.cFileName;
            mtx.lock();
            wcout << L"ID thread: " << this_thread::get_id()
                << L" path: " << filePath << endl;
            mtx.unlock();
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // если это директория
                filePath += L"\\";
                //thread* th = new thread(SearchForDir, filePath, name, ref(pathRezuld));
                qTreadsLocal.push(new thread(SearchForDir, filePath, name, ref(pathRezuld)));
                // сканируем директорию в новом потоке
            } else if (!wcsncmp(findData.cFileName, name.c_str(), name.size())) {
                // если есть совпадение с искомым именем файла
                mtx2.lock();
                pathRezuld.push(filePath);
                mtx2.unlock();
                mtx.lock();
                wcout << L"HIT!" << endl;
                mtx.unlock();
            }
        } catch (exception e) {
            mtx.lock();
            wcout << L"Ошибка: " << e.what() << L" Path: " << path << endl;
            mtx.unlock();
        }
    } while (FindNextFile(hf, &findData) != 0);
    //закрываем потоки
    FindClose(hf);
    while (!qTreadsLocal.empty()){
        qTreadsLocal.front()->join();
        qTreadsLocal.pop();
    }
}

// конвертация массива char в wchar
wchar_t* ConvertChartoWchar(const char* ch) {
    const size_t cSize = strlen(ch) + 1;
    auto wch = new wchar_t[cSize];
    mbstowcs(wch, ch, cSize);
    return wch;
}

int main() {
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U8TEXT);
    wstring fname;
    queue<wstring> Paths; // очередь из имен путей к файлу
    queue<thread*> qTreads; // очередь потоков
    wcout << L"Введите название файла\n: ";
    wcin >> fname;

    char buf[26];
    GetLogicalDriveStringsA(sizeof(buf), buf); // получам список дисков установленных в стему
    for (char* s = buf; *s; s += strlen(s) + 1) {
        wstring disk(ConvertChartoWchar(s));
        //начинам сканировать диски
        qTreads.push(new thread(SearchForDir, disk, fname, ref(Paths)));
    }
    //закрываем потоки
    while (!qTreads.empty()) {
        qTreads.front()->join();
        qTreads.pop();
    }
    wcout << L"\nFILE PATH:\n";

    if (Paths.empty()) {
        wcout << L"Файл не найден\n";
        cin.get();
        return 0;
    }

    queue<wstring> TestPaths = Paths;
    for (unsigned int i = 1; !TestPaths.empty(); ++i) {
        wcout << i << L") " << TestPaths.front() << endl;
        TestPaths.pop();
    }
    int mod;
    while (true) {
        wcout << L"С каким файлом работать?\n:";
        wcin >> mod;
        if (1 <= mod && mod <= Paths.size() && cin.good()) break;
        wcout << L"Ошибка ввода\n";
    }

    for (unsigned int i = 1; i < mod && !Paths.empty(); ++i) Paths.pop();
    wstring path = Paths.front();
    WIN32_FIND_DATA findData;
    HANDLE fData = FindFirstFileW(path.c_str(), &findData);

    LONGLONG nFileLen = (findData.nFileSizeHigh * (MAXDWORD + 1)) + findData.nFileSizeLow;
    wcout << L"Размер файла: " << nFileLen << L" байт" << endl;

    SYSTEMTIME sm;
    FileTimeToSystemTime(&findData.ftCreationTime, &sm);
    wprintf(L"Время создания: %d.%d.%d %d:%d", sm.wDay, sm.wMonth, sm.wYear, sm.wHour, sm.wMinute);

    // проверяем не является ли файл системным
    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DEVICE || //Это значение зарезервировано для использования системы.
        findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM ||
        //Файл или каталог, в который операционная система использует часть или используется исключительно.
        findData.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY || // Файл, используемый для временного хранилища.
        findData.dwFileAttributes & FILE_ATTRIBUTE_VIRTUAL || // Это значение зарезервировано для использования системы.
        findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {  //Файл или каталог скрыт.Он не включен в обычный список каталогов.
        wcout << L"Файл системный\n";
    } else {
        wcout << L"Файл не системный, атрибут: " << findData.dwFileAttributes;
        while (true) {
            wcout << L"\nПеременовать файл? Y/N\n:";
            wstring input;
            wcin >> input;
            if (input == L"y" || input == L"Y" || input == L"yes" ||
                       input == L"YES" || input == L"Yes" && cin.good()){
                wcout << L"Введите новое название файла\n:";
                wcin >> input;
                if (!CopyFileW(path.c_str(), (path.substr(0, path.size() - wcslen(findData.cFileName)) + input).c_str(), 0)||
                    !DeleteFileW(path.c_str()))  {
                    wcout << L"Не удалось переминовть файл";
                }
                break;
            } else if (input == L"n" || input == L"N" || input == L"no" ||
                       input == L"NO" || input == L"No" && cin.good()){
                break;
            }
            wcout << L"Не понятный ввод\n";
        }
    }
    FindClose(fData);
    cin.get();
    return 0;
}
