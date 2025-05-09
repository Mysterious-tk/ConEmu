﻿
/*
Copyright (c) 2009-present Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define HIDE_USE_EXCEPTION_INFO
#include "../common/defines.h"
#include "Header.h"
#include "SettingsStorage.h"
#include "ConEmu.h"

#include "../common/WFiles.h"

#define RAPIDXML_NO_STDLIB
#define RAPIDXML_NO_STREAMS
#define assert(x) _ASSERTE(x)
#include "../modules/rapidxml/rapidxml.hpp"
#include "../modules/rapidxml/rapidxml_print.hpp"
using namespace rapidxml;
// catch (rapidxml::parse_error& e)

#ifdef _DEBUG
#define HEAPVAL //HeapValidate(GetProcessHeap(), 0, nullptr);
#else
#define HEAPVAL
#endif

// Let save integers as 10-based numbers
#define REG__LONG  254
#define REG__ULONG 255

SettingsBase::SettingsBase(const SettingsStorage& Storage)
{
	m_Storage = Storage;
}

SettingsBase::SettingsBase()
{
	memset(&m_Storage,0,sizeof(m_Storage));
}

SettingsBase::~SettingsBase()
{
}

// Helpers: Don't change &value if it was not loaded
bool SettingsBase::Load(const wchar_t *regName, char  &value)
{
	return Load(regName, reinterpret_cast<LPBYTE>(&value), 1);
}
bool SettingsBase::Load(const wchar_t *regName, BYTE  &value)
{
	return Load(regName, static_cast<LPBYTE>(&value), 1);
}
bool SettingsBase::Load(const wchar_t *regName, DWORD &value)
{
	return Load(regName, reinterpret_cast<LPBYTE>(&value), sizeof(DWORD));
}
bool SettingsBase::Load(const wchar_t *regName, LONG  &value)
{
	return Load(regName, reinterpret_cast<LPBYTE>(&value), sizeof(DWORD));
}
bool SettingsBase::Load(const wchar_t *regName, int   &value)
{
	return Load(regName, reinterpret_cast<LPBYTE>(&value), sizeof(DWORD));
}
bool SettingsBase::Load(const wchar_t *regName, UINT  &value)
{
	return Load(regName, reinterpret_cast<LPBYTE>(&value), sizeof(DWORD));
}
// Don't change &value if it was not loaded
bool SettingsBase::Load(const wchar_t *regName, bool  &value)
{
	BYTE byteVal = 0;
	const bool bRc = Load(regName, &byteVal, sizeof(byteVal));
	if (bRc)
		value = (byteVal!=0);
	return bRc;
}
// Don't change &value if it was not loaded
bool SettingsBase::Load(const wchar_t *regName, RECT &value)
{
	wchar_t szRect[80] = L"";
	// Example: "100,200,500,400" (Left,Top,Right,Bottom)
	if (!Load(regName, szRect, countof(szRect)-1))
		return false;
	RECT rc = {};
	wchar_t *psz = szRect, *pszEnd;
	for (int i = 0; i < 4; i++)
	{
		if (!isDigit(psz[0]) && (psz[0] != L'-'))
			return false;
		reinterpret_cast<LONG*>(&rc)[i] = wcstol(psz, &pszEnd, 10);
		if (i < 3)
		{
			if (!pszEnd || !wcschr(L",;", *pszEnd))
				return false;
			psz = pszEnd+1;
		}
	}
	if (IsRectEmpty(&rc))
		return false;
	value = rc;
	return true;
}

void SettingsBase::Save(const wchar_t *regName, const wchar_t *value)
{
	if (!value) value = L"";  // protect against nullptr values
	Save(regName, reinterpret_cast<LPCBYTE>(value), REG_SZ, (_tcslen(value)+1)*sizeof(wchar_t));
}

// Use strict types to prohibit unexpected template usage
void SettingsBase::Save(const wchar_t *regName, const char&  value)
{
	_Save(regName, value);
}
void SettingsBase::Save(const wchar_t *regName, const bool&  value)
{
	_Save(regName, value);
}
void SettingsBase::Save(const wchar_t *regName, const BYTE&  value)
{
	_Save(regName, value);
}
void SettingsBase::Save(const wchar_t *regName, const DWORD& value)
{
	Save(regName, LPBYTE(&value), REG_DWORD, sizeof(DWORD));
}
void SettingsBase::Save(const wchar_t *regName, const LONG&  value)
{
	Save(regName, LPBYTE(&value), REG__LONG, sizeof(int));
}
void SettingsBase::Save(const wchar_t *regName, const int&   value)
{
	_ASSERTE(sizeof(int) == 4);
	Save(regName, LPBYTE(&value), REG__LONG, sizeof(int));
}
void SettingsBase::Save(const wchar_t *regName, const UINT&   value)
{
	_ASSERTE(sizeof(int) == 4);
	Save(regName, LPBYTE(&value), REG__ULONG, sizeof(int));
}
void SettingsBase::Save(const wchar_t *regName, const RECT&  value)
{
	wchar_t szRect[80];
	swprintf_c(szRect, L"%i,%i,%i,%i", value.left, value.top, value.right, value.bottom);
	Save(regName, szRect);
}

// nSize in BYTES!!!
void SettingsBase::SaveMSZ(const wchar_t *regName, const wchar_t *value, DWORD nSize)
{
	if (!value || !*value)
		Delete(regName);
	else
		Save(regName, LPBYTE(value), REG_MULTI_SZ, nSize);
}



SettingsRegistry::SettingsRegistry()
	: SettingsBase()
{
	m_Storage = {StorageType::REG};
	regMy = nullptr;
}

SettingsRegistry::~SettingsRegistry()
{
	SettingsRegistry::CloseKey();
}



// ReSharper disable once CppParameterMayBeConst
bool SettingsRegistry::OpenKey(HKEY inHKEY, const wchar_t *regPath, const unsigned access, BOOL abSilent /*= FALSE*/)
{
	bool res = false;

	_ASSERTE(!gpConEmu->IsResetBasicSettings() || ((access & KEY_WRITE)!=KEY_WRITE) || !lstrcmpi(regPath, L"Software\\Microsoft\\Command Processor"));

	if ((access & KEY_WRITE) == KEY_WRITE)
		res = RegCreateKeyEx(inHKEY, regPath, 0, nullptr, 0, access, nullptr, &regMy, nullptr) == ERROR_SUCCESS;
	else
		res = RegOpenKeyEx(inHKEY, regPath, 0, access, &regMy) == ERROR_SUCCESS;

	return res;
}
bool SettingsRegistry::OpenKey(const wchar_t *regPath, unsigned access, BOOL abSilent /*= FALSE*/)
{
	return OpenKey(HKEY_CURRENT_USER, regPath, access);
}
void SettingsRegistry::CloseKey()
{
	if (regMy)
	{
		RegCloseKey(regMy);
		regMy = nullptr;
	}
}



bool SettingsRegistry::Load(const wchar_t *regName, LPBYTE value, DWORD nSize)
{
	_ASSERTE(nSize>0);

	DWORD nNewSize = nSize;
	LONG lRc = RegQueryValueEx(regMy, regName, nullptr, nullptr, static_cast<LPBYTE>(value), &nNewSize);
	if (lRc == ERROR_SUCCESS)
		return true;

	// Access denied может быть, если пытаемся _читать_ из ключа, открытого на _запись_
	_ASSERTE(lRc != ERROR_ACCESS_DENIED);

	if (lRc == ERROR_MORE_DATA && nSize == sizeof(BYTE) && nNewSize == sizeof(DWORD))
	{
		// если тип раньше был DWORD а стал - BYTE
		DWORD nData = 0;
		lRc = RegQueryValueEx(regMy, regName, nullptr, nullptr, reinterpret_cast<LPBYTE>(&nData), &nNewSize);
		if (lRc == ERROR_SUCCESS)
		{
			*value = static_cast<BYTE>(nData & 0xFF);
			return true;
		}
	}
	return false;
}
// эта функция, если значения нет (или тип некорректный) *value НЕ трогает
bool SettingsRegistry::Load(const wchar_t *regName, wchar_t **value)
{
	DWORD len = 0;

	if (RegQueryValueExW(regMy, regName, nullptr, nullptr, nullptr, &len) == ERROR_SUCCESS && len)
	{
		int nChLen = static_cast<int>(len) / 2;
		if (*value) {free(*value); *value = nullptr;}
		*value = static_cast<wchar_t*>(malloc((nChLen+2)*sizeof(wchar_t)));

		const bool lbRc = (RegQueryValueExW(regMy, regName, nullptr, nullptr, reinterpret_cast<LPBYTE>(*value), &len) == ERROR_SUCCESS);

		if (!lbRc)
			nChLen = 0;

		(*value)[nChLen] = 0;
		(*value)[nChLen+1] = 0; // in case of REG_MULTI_SZ

		return lbRc;
	}
	//else if (!*value)
	//{
	//	*value = (wchar_t*)malloc(sizeof(wchar_t)*2);
	//	(*value)[0] = 0; (*value)[1] = 0; // На случай REG_MULTI_SZ
	//}

	return false;
}
// эта функция, если значения нет (или тип некорректный) value НЕ трогает
bool SettingsRegistry::Load(const wchar_t *regName, wchar_t *value, int maxLen)
{
	_ASSERTE(maxLen>1);
	DWORD len = 0, dwType = 0;

	if (RegQueryValueExW(regMy, regName, nullptr, &dwType, nullptr, &len) == ERROR_SUCCESS && dwType == REG_SZ)
	{
		len = maxLen*2; // max size in BYTES

		if (RegQueryValueExW(regMy, regName, nullptr, nullptr, reinterpret_cast<LPBYTE>(value), &len) == ERROR_SUCCESS)
		{
			if (value)
				value[maxLen-1] = 0; // to ensure ASCIIZ
			return true;
		}
	}

	return false;
}



void SettingsRegistry::Delete(const wchar_t *regName)
{
	RegDeleteValue(regMy, regName);
}

void SettingsRegistry::DeleteKey(const wchar_t *regName)
{
	RegDeleteKey(regMy, regName);
}



void SettingsRegistry::Save(const wchar_t *regName, LPCBYTE value, const DWORD nType, const DWORD nSize)
{
	_ASSERTE(value && nSize);
	RegSetValueEx(regMy, regName, 0,
		(nType == REG__LONG || nType == REG__ULONG) ? REG_DWORD : nType,
		const_cast<LPBYTE>(value), nSize);
}



















/* *************************** */

SettingsINI::SettingsINI(const SettingsStorage& Storage)
	: SettingsBase(Storage)
{
	mpsz_Section = nullptr;
	mpsz_IniFile = nullptr;
	m_Storage.Type = StorageType::INI;
}
SettingsINI::~SettingsINI()
{
	SettingsINI::CloseKey();
}



bool SettingsINI::OpenKey(const wchar_t *regPath, unsigned access, BOOL abSilent /*= FALSE*/)
{
	_ASSERTE(!gpConEmu->IsResetBasicSettings() || ((access & KEY_WRITE)!=KEY_WRITE));

	SafeFree(mpsz_Section);

	if (!regPath || !*regPath)
	{
		mpsz_IniFile = nullptr;
		return false;
	}

	if (m_Storage.hasFile())
	{
		mpsz_IniFile = m_Storage.File;
	}
	else
	{
		_ASSERTE(m_Storage.hasFile());
		m_Storage.File = mpsz_IniFile = gpConEmu->ConEmuIni();
	}

	if (!mpsz_IniFile || !*mpsz_IniFile)
	{
		mpsz_IniFile = nullptr;
		return false;
	}

	// ReSharper disable once CppLocalVariableMayBeConst
	HANDLE hFile = CreateFile(mpsz_IniFile,
							  ((access & KEY_WRITE) == KEY_WRITE) ? GENERIC_WRITE : GENERIC_READ,
							  FILE_SHARE_READ,
							  nullptr,
							  ((access & KEY_WRITE) == KEY_WRITE) ? OPEN_ALWAYS : OPEN_EXISTING,
							  FILE_ATTRIBUTE_NORMAL,
							  nullptr);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		mpsz_IniFile = nullptr;
		return false;
	}

	CloseHandle(hFile);

	wchar_t* pszDup = CEStr(regPath).Detach();
	if (!pszDup)
	{
		mpsz_IniFile = nullptr;
		return FALSE;
	}
	// Замена символов
	wchar_t* psz = pszDup;
	while ((psz = wcspbrk(psz, L" ?")))
		*psz = L'_';
	psz = pszDup;
	while ((psz = wcspbrk(psz, L"/\\")))
		*psz = L' ';


	const size_t cchMax = (_tcslen(pszDup)*3)+1;
	char* pszSectionA = static_cast<char*>(malloc(cchMax));
	if (!pszSectionA)
	{
		mpsz_IniFile = nullptr;
		SafeFree(pszDup);
		return false;
	}

	const int nLen = WideCharToMultiByte(CP_UTF8, 0, pszDup, -1, pszSectionA, cchMax, nullptr, nullptr);
	mpsz_Section = static_cast<wchar_t*>(malloc((nLen+1)*sizeof(*mpsz_Section)));
	if (!mpsz_Section)
	{
		SafeFree(pszSectionA);
		SafeFree(pszDup);
		return false;
	}
	MultiByteToWideChar(CP_ACP, 0, pszSectionA, -1, mpsz_Section, nLen+1);

	SafeFree(pszSectionA);
	SafeFree(pszDup);
	return true;
}
void SettingsINI::CloseKey()
{
	SafeFree(mpsz_Section);
}



bool SettingsINI::Load(const wchar_t *regName, LPBYTE value, DWORD nSize)
{
	_ASSERTE(nSize>0);

	return false;
}
// эта функция, если значения нет (или тип некорректный) *value НЕ трогает
bool SettingsINI::Load(const wchar_t *regName, wchar_t **value)
{
	//DWORD len = 0;


	return false;
}
// эта функция, если значения нет (или тип некорректный) value НЕ трогает
bool SettingsINI::Load(const wchar_t *regName, wchar_t *value, int maxLen)
{
	_ASSERTE(maxLen>1);
	//DWORD len = 0, dwType = 0;

	return false;
}



void SettingsINI::Delete(const wchar_t *regName)
{
	if (mpsz_IniFile && *mpsz_IniFile && mpsz_Section && *mpsz_Section && regName && *regName)
	{
		//char szName[MAX_PATH*3] = {};
		//WideCharToMultiByte(CP_UTF8, 0, regName, -1, szName, countof(szName), 0,0);
		//if (*szName)
		WritePrivateProfileString(mpsz_Section, regName, nullptr, mpsz_IniFile);
	}
}

void SettingsINI::DeleteKey(const wchar_t *regName)
{
	// #INI Erase section from .ini file
}


void SettingsINI::Save(const wchar_t *regName, LPCBYTE value, const DWORD nType, const DWORD nSize)
{
	_ASSERTE(value && nSize);
}






/* *************************** */
namespace
{
	class FileException : public std::exception
	{
		int mn_ErrCode;
	public:
		FileException(DWORD nErrCode)
			: mn_ErrCode(nErrCode)
		{
		};

		DWORD ErrorCode() const
		{
			return mn_ErrCode;
		};
	};

	class FileWriter
	{
		HANDLE hFile;
		friend class iterator;
		MArray<char> data;

	public:
		FileWriter() = delete;
		FileWriter(const FileWriter&) = delete;
		FileWriter(FileWriter&&) = delete;
		FileWriter& operator=(const FileWriter&) = delete;
		FileWriter& operator=(FileWriter&&) = delete;

		// ReSharper disable once CppNonExplicitConvertingConstructor
		// ReSharper disable once CppParameterMayBeConst
		FileWriter(LPCWSTR pszXmlFile)
		{
			hFile = CreateFile(pszXmlFile, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (!hFile || hFile == INVALID_HANDLE_VALUE)
				throw FileException(GetLastError());
			data.reserve(128*1024);
		};

		~FileWriter()
		{
			if (hFile && hFile != INVALID_HANDLE_VALUE)
				CloseHandle(hFile);
		};

		void Write()
		{
			if (data.size() > 0)
			{
				DWORD nWritten = 0;
				const BOOL written = WriteFile(hFile, &data[0], data.size(), &nWritten, nullptr);
				if (!written || !nWritten)
					throw FileException(GetLastError());
				SetEndOfFile(hFile);
			}
		};

		class iterator
		{
			FileWriter* writer;
			INT_PTR index;

		public:
			iterator(FileWriter* _writer)
				: writer(_writer)
				, index()
			{
			};

			char& operator*() const
			{
				if (index <= writer->data.size())
					writer->data.push_back(0);
				return writer->data[index];
			};

			// ++prefix
			iterator& operator++()
			{
				++index;
				return *this;
			};

			// postfix++
			iterator operator++(int)
			{
				const iterator prev(*this);
				++index;
				return prev;
			};
		};

		iterator CreateIterator()
		{
			return iterator(this);
		};
	};
};


/* *************************** */
SettingsXML::SettingsXML(const SettingsStorage& Storage)
	: SettingsBase(Storage)
{
	m_Storage.Type = StorageType::XML;
}
SettingsXML::~SettingsXML()
{
	CloseStorage();
}

// The function converts UTF-8 to UCS2 (CEStr) and returns pointer to allocated buffer (CEStr)
LPCWSTR SettingsXML::utf2wcs(const char* utf8, CEStr& wc)
{
	if (!utf8)
	{
		_ASSERTE(utf8 != nullptr);
		wc.Release();
		return nullptr;
	}

	wc.Clear();
	int wcLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
	if (wcLen > 0)
	{
		wcLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wc.GetBuffer(wcLen), wcLen);
		if (wcLen <= 0)
			wc.Clear();
	}
	return wc.c_str();
}

// Just a wrapper for WideCharToMultiByte
const char* SettingsXML::wcs2utf(const wchar_t* wc, CEStrA& str)
{
	str.Release();
	int ucLen = WideCharToMultiByte(CP_UTF8, 0, wc, -1, nullptr, 0, nullptr, nullptr);
	if (!str.GetBuffer(ucLen))
	{
		_ASSERTE(str.ms_Val!=nullptr);
		return nullptr;
	}
	ucLen = WideCharToMultiByte(CP_UTF8, 0, wc, -1, str.data(), ucLen, nullptr, nullptr);
	if (ucLen <= 0)
	{
		_ASSERTE(ucLen > 0);
		str.Release();
		return nullptr;
	}
	return str.ms_Val;
}

// Prepare UTF-8 string to write into xml DOM
const char* SettingsXML::wcs2utf(const wchar_t* wc) const
{
	if (!wc || !*wc)
	{
		_ASSERTE(wc!=nullptr);
		return "";
	}

	if (!mp_File)
	{
		_ASSERTE(mp_File!=nullptr);
		return nullptr;
	}

	CEStrA temp;
	if (!wcs2utf(wc, temp))
	{
		return nullptr;
	}

	const char* allocated = mp_File->allocate_string(temp);
	_ASSERTE(allocated != nullptr);
	return allocated;
}

bool SettingsXML::OpenStorage(unsigned access, CEStr& pszErr)
{
	bool bRc = false;
	bool bNeedReopen = (mp_File == nullptr);
	LPCWSTR pszXmlFile = nullptr;
	const bool bAllowCreate = (access & KEY_WRITE) == KEY_WRITE;

	_ASSERTE(pszErr == nullptr);

	if (m_Storage.hasFile())
	{
		pszXmlFile = m_Storage.File;
	}
	else
	{
		// Must be already initialized
		_ASSERTE(m_Storage.hasFile());
		m_Storage.File = pszXmlFile = gpConEmu->ConEmuXml();
		if (!pszXmlFile || !*pszXmlFile)
		{
			goto wrap;
		}
	}

	// Changed access type?
	if (bNeedReopen || (mn_access != access))
	{
		DWORD dwAccess = GENERIC_READ;
		DWORD dwMode = OPEN_EXISTING;

		if ((access & KEY_WRITE) == KEY_WRITE)
		{
			dwAccess |= GENERIC_WRITE;
			dwMode = OPEN_ALWAYS;
		}

		HANDLE hFile = CreateFile(pszXmlFile, dwAccess, FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, dwMode, 0, nullptr);

		// XML-file is absent
		if (!hFile || (hFile == INVALID_HANDLE_VALUE))
		{
			goto wrap;
		}
		else
		{
			BY_HANDLE_FILE_INFORMATION bfi = {};
			if (GetFileInformationByHandle(hFile, &bfi))
			{
				mb_Empty = (bfi.nFileSizeHigh == 0 && bfi.nFileSizeLow == 0);
				if (!mb_Empty)
				{
					// UTF-8 BOM? MsXmlDOM does not allows BOM
					if (bfi.nFileSizeHigh == 0 && bfi.nFileSizeLow == 3)
					{
						BYTE bom[3] = {}; DWORD nRead = 0;
						if (ReadFile(hFile, bom, sizeof(bom), &nRead, nullptr) && (nRead == sizeof(bom))
							&& (bom[0]==0xEF && bom[1]==0xBB && bom[2]==0xBF))
						{
							mb_Empty = true;
						}
					}
				}
			}
			CloseHandle(hFile); hFile = nullptr;
			if (mb_Empty && bAllowCreate)
			{
				bNeedReopen = true;
				hFile = CreateFile(pszXmlFile, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
				if (!hFile || (hFile == INVALID_HANDLE_VALUE))
				{
					goto wrap;
				}
				else
				{
					// ReSharper disable once CppLocalVariableMayBeConst
					LPCSTR pszDefault = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
						"<key name=\"Software\">\r\n"
						"\t<key name=\"ConEmu\">\r\n"
						"\t</key>\r\n"
						"</key>\r\n";
					DWORD nLen = lstrlenA(pszDefault);
					WriteFile(hFile, pszDefault, nLen, &nLen, nullptr);
					CloseHandle(hFile);
				}
				hFile = nullptr;
			}
		}
	}

	if (!bNeedReopen)
	{
		bRc = true;
		goto wrap;
	}

	if (mb_Empty && !bAllowCreate)
		goto wrap;

	CloseStorage();

	{
		_ASSERTE(mp_File == nullptr);
		_ASSERTE(mp_Utf8Data.data() == nullptr);

		DWORD cchChars = 0, nErrCode = 0;
		if (ReadTextFile(pszXmlFile, static_cast<DWORD>(-1), mp_Utf8Data, cchChars, nErrCode) < 0)
		{
			goto wrap;
		}

		try
		{
			mp_File = new document;
			mp_File->parse<parse_declaration_node|parse_comment_nodes>(mp_Utf8Data.data());
			// <?xml version="1.0" encoding="utf-8"?>
			node* decl = mp_File->first_node();
			if (!decl || (decl->type() != node_declaration))
			{
				decl = mp_File->allocate_node(node_declaration);
				SetAttr(decl, "version", "1.0");
				SetAttr(decl, "encoding", "utf-8");
				mp_File->insert_node(mp_File->first_node(), decl);
			}
			mn_access = access;
			bRc = true;
		}
		catch (rapidxml::parse_error& e)
		{
			CEStr lsWhat, lsWhere;
			pszErr = CEStr(
				L"Exception in SettingsXML::OpenStorage\r\n",
				((access & KEY_WRITE) != KEY_WRITE)
					? L"Failed to load configuration file!\r\n"
					: L"Failed to open configuration file for writing!\r\n",
				pszXmlFile,
				utf2wcs(e.what(), lsWhat), utf2wcs(e.where<char>(), lsWhere));
			bRc = false;
		}
	}

wrap:
	if (!bRc)
		CloseStorage();
	return bRc;
}

bool SettingsXML::OpenKey(const wchar_t *regPath, unsigned access, BOOL abSilent /*= FALSE*/) noexcept
{
	// That may occur if Basic settings and "Export" button was pressed
	_ASSERTE(!gpConEmu->IsResetBasicSettings() || ((access & KEY_WRITE)!=KEY_WRITE));

	bool lbRc = false;
	CEStr pszErr = nullptr;
	wchar_t szName[MAX_PATH];
	const wchar_t* psz = nullptr;
	SettingsXML::node* pKey = nullptr;
	const bool bAllowCreate = (access & KEY_WRITE) == KEY_WRITE;

	CloseKey(); // JIC

	if (!regPath || !*regPath)
	{
		return false;
	}

	if (!OpenStorage(access, pszErr))
	{
		goto wrap;
	}

	try
	{
		_ASSERTE(mp_File != nullptr);

		pKey = mp_File;

		if (!pKey)
		{
			const wchar_t szRootError[] = L"XML: Root node not found!";
			pszErr = CEStr(szRootError, m_Storage.File, L"\r\n", regPath);
			goto wrap;
		}

		while (*regPath)
		{
			// Получить следующий токен
			psz = wcschr(regPath, L'\\');

			if (!psz)
				psz = regPath + _tcslen(regPath);

			lstrcpyn(szName, regPath, psz-regPath+1);
			// Найти в структуре XML
			pKey = FindItem(pKey, L"key", szName, bAllowCreate);

			if (!pKey)
			{
				if (bAllowCreate)
				{
					pszErr = CEStr(L"XML: Can't create key <", szName, L">!");
				}
				else
				{
					//swprintf_c(szErr, L"XML: key <%s> not found!", szName);
					// Don't show error - use default settings
					pszErr.Release();
				}

				goto wrap;
			}

			if (*psz == L'\\')
			{
				regPath = psz + 1;
			}
			else
			{
				break;
			}
		}

		// Нашли, запомнили
		mp_Key = pKey; pKey = nullptr;

		lbRc = true;

	}
	catch (rapidxml::parse_error& e)
	{
		CEStr lsWhat, lsWhere;
		pszErr = CEStr(L"Exception in SettingsXML::OpenKey\r\n", m_Storage.File,
			L"\r\n", regPath, L"\r\n",
			utf2wcs(e.what(), lsWhat), L"\r\n", utf2wcs(e.where<char>(), lsWhere));
		lbRc = false;
	}

wrap:
	if (!lbRc && !pszErr.IsEmpty() && !abSilent)
	{
		// Don't show error message box as a child of ghWnd
		// otherwise it may be closed unexpectedly on exit
		MsgBox(pszErr, MB_ICONSTOP, nullptr, nullptr);
	}

	return lbRc;
}

void SettingsXML::SetDataChanged()
{
	mb_DataChanged = true;
}

void SettingsXML::TouchKey(SettingsXML::node* apKey) noexcept
{
	try
	{
		SettingsXML::node* key = apKey;
		// Touch only <key> nodes
		while (key)
		{
			const char* name = key->name();
			if (!name || !*name)
				return;
			if (strcmp(name, "key") == 0)
				break;
			key = key->parent();
		}
		if (!key || key == mp_File)
			return;

		SYSTEMTIME st; wchar_t szTime[32];
		GetLocalTime(&st);
		swprintf_c(szTime, L"%04i-%02i-%02i %02i:%02i:%02i",
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		SetAttr(key, "modified", szTime);
		SetAttr(key, "build", gpConEmu->ms_ConEmuBuild);
	}
	catch (rapidxml::parse_error&)
	{
		// #XML Log error
	}
}

void SettingsXML::CloseStorage() noexcept
{
	HANDLE hFile = nullptr;
	bool bCanSave = false;
	HANDLE hMutex = CreateMutex(nullptr, FALSE, L"ConEmu_SettingsXML_Mutex");
    WaitForSingleObject(hMutex, INFINITE);

	CloseKey();

	if (mb_Modified && mp_File)
	{
		// Путь к файлу проинициализирован в OpenKey
		_ASSERTE(m_Storage.hasFile());
		// ReSharper disable once CppLocalVariableMayBeConst
		LPCWSTR pszXmlFile = m_Storage.File;

		if (pszXmlFile)
		{
			hFile = CreateFile(pszXmlFile, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
			                   nullptr, OPEN_EXISTING, 0, nullptr);

			// XML-файл отсутсвует, или ошибка доступа
			if (hFile == INVALID_HANDLE_VALUE)
			{
				const DWORD dwErrCode = GetLastError();
				wchar_t szErr[MAX_PATH*2];
				swprintf_c(szErr, L"Can't open file for writing!\n%s\nErrCode=0x%08X",
				          pszXmlFile, dwErrCode);
				MBoxA(szErr);
			}
			else
			{
				CloseHandle(hFile); hFile = nullptr;
				bCanSave = true;
			}

			if (bCanSave)
			{
				try
				{
					MCHKHEAP;
					FileWriter file(pszXmlFile);
					// Don't write BOM explicitly?
					MCHKHEAP;
					rapidxml::print(file.CreateIterator(), *mp_File);
					MCHKHEAP;
					file.Write();
					MCHKHEAP;
				}
				catch (FileException& e)
				{
					wchar_t szErrCode[32];
					const CEStr szErr(
						L"Failed to write configuration file, Code=",
						ultow_s(e.ErrorCode(), szErrCode, 10),
						L"\r\n", pszXmlFile);
					MBoxA(szErr);
				}
				catch (rapidxml::parse_error& e)
				{
					CEStr lsWhat, lsWhere;
					const CEStr szErr(
						L"Failed to write configuration file!\r\n",
						pszXmlFile,
						utf2wcs(e.what(), lsWhat), utf2wcs(e.where<char>(), lsWhere));
					MBoxA(szErr);
				}
			}
		}
	}

	mp_Utf8Data.Release();

	SafeDelete(mp_File);

	mb_Modified = false;
	mb_DataChanged = false;
	mb_Empty = false;
	mn_access = 0;
	ReleaseMutex(hMutex);
    CloseHandle(hMutex);
}

void SettingsXML::CloseKey() noexcept
{
	if (mp_Key && mb_DataChanged)
	{
		TouchKey(mp_Key);
	}
	mb_DataChanged = false;
	mp_Key = nullptr;
}

// Function DOES NOT return nullptr
const char* SettingsXML::GetAttr(SettingsXML::node* apNode, const char* asName)
{
	attribute* attr = apNode ? apNode->first_attribute(asName, 0, false) : nullptr;
	const char* value = attr ? attr->value() : nullptr;
	return value ? value : "";
}

bool SettingsXML::SetAttr(SettingsXML::node* apNode, const char* asName, const wchar_t* asValue)
{
	bool lbRc = false;

	if (apNode)
		lbRc = SetAttr(apNode, asName, (asValue && *asValue) ? wcs2utf(asValue) : nullptr);

	return lbRc;
}

bool SettingsXML::SetAttr(SettingsXML::node* apNode, const char* asName, const char* asValue)
{
	bool lbRc = false;

	if (apNode)
	{
		static char empty[] = "";
		const char* new_value = asValue ? asValue : empty;
		attribute* attr = apNode->first_attribute(asName, 0, false);
		if (attr)
		{
			const char* old_value = attr->value();
			if (!old_value || (strcmp(old_value, new_value) != 0))
				SetDataChanged();
			attr->value(new_value);
		}
		else
		{
			SetDataChanged();
			attr = mp_File->allocate_attribute(asName, new_value);
			apNode->append_attribute(attr);
		}
		lbRc = true;
	}

	return lbRc;
}

SettingsXML::node* SettingsXML::FindItem(SettingsXML::node* apFrom, const wchar_t* asType, const wchar_t* asName, bool abAllowCreate)
{
	if (!mp_File || !apFrom || !asType || !*asType || !asName || !*asName)
	{
		_ASSERTE(mp_File && apFrom && asType && *asType && asName && *asName);
		return nullptr;
	}

	CEStrA sType, sName;
	if (!wcs2utf(asType, sType) || !wcs2utf(asName, sName))
	{
		return nullptr;
	}

	node* pChild = apFrom->first_node(sType);
	while (pChild)
	{
		const char* name = GetAttr(pChild, "name");
		if (lstrcmpiA(name, sName) == 0)
			return pChild;
		pChild = pChild->next_sibling(sType);
	}

	if (!pChild && abAllowCreate)
	{
		pChild = mp_File->allocate_node(node_element, wcs2utf(asType));
		if (pChild)
		{
			SetAttr(pChild, "name", asName);
			TouchKey(pChild);
			apFrom->append_node(pChild);
		}
	}

	return pChild;
}

// Supports MultiLine and SingleLine strings
// If there were no key_value or key_value_type is unexpected, DOESN'T touch *value
bool SettingsXML::Load(const wchar_t *regName, wchar_t **value) noexcept
{
	bool lbRc = false;
	SettingsXML::node *pChild = nullptr;
	SettingsXML::node *pNode = nullptr;
	// ReSharper disable once CppJoinDeclarationAndAssignment
	const char *sType;
	size_t cchMax = 0;
	CEStrA data;
	LPCSTR pszData = nullptr;

	try
	{
		if (mp_Key)
			pChild = FindItem(mp_Key, L"value", regName, false);

		if (!pChild)
			return false;

		// ReSharper disable once CppJoinDeclarationAndAssignment
		sType = GetAttr(pChild, "type");

		if (0 == _strcmpi(sType, "multi"))
		{
			//<value name="CmdLineHistory" type="multi">
			//	<line data="C:\Far\Far.exe"/>
			//	<line data="cmd"/>
			//</value>
			struct line_info { const char* str; size_t cchSize; };
			MArray<line_info> lines;

			for (pNode = pChild->first_node("line"); pNode; pNode = pNode->next_sibling("line"))
			{
				const char* lstr = GetAttr(pNode, "data");
				if (!lstr || !*lstr)
					lstr = " "; // Because of ASCIIZZ we must be sure there were no zero-length string
				const size_t cchSize = strlen(lstr) + 1;
				cchMax += cchSize;
				lines.push_back({lstr, cchSize});
			}

			if (!lines.empty())
			{
				// ASCIIZ,ASCIIZ,...,ASCIIZ buffer
				if (char* buffer = data.GetBuffer(cchMax))
				{
					for (auto& line : lines)
					{
						strcpy_s(buffer, line.cchSize, line.str);
						buffer += line.cchSize;
					}
					_ASSERTE(cchMax>=1 && data.ms_Val[cchMax-1]==0);
					pszData = data.c_str();
				}
			}
		}
		else if (0 == _strcmpi(sType, "string"))
		{
			pszData = GetAttr(pChild, "data");
			cchMax = pszData ? (strlen(pszData) + 1) : 0; // ASCIIZ
		}
		else
		{
			// We don't care about other data types, only strings here
		}

		// Data exist?
		if (pszData)
		{
			const int cvtSize = MultiByteToWideChar(CP_UTF8, 0, pszData, cchMax, nullptr, 0);
			if (cvtSize > 0)
			{
				// Allocate data for ASCIIZZ (additional zero at the end)
				if ((*value = static_cast<wchar_t*>(realloc(*value, (cvtSize + 1) * sizeof(**value)))))
				{
					const int cvt = MultiByteToWideChar(CP_UTF8, 0, pszData, cchMax, *value, cvtSize+1);
					_ASSERTE(cvt == cvtSize);
					if (cvt > 0)
					{
						_ASSERTE((*value)[cvt-1] == 0);
						(*value)[cvt] = 0; // ASCIIZZ
						lbRc = true;
					}
				}
			}
		}
	}
	catch (rapidxml::parse_error&)
	{
		// #XML Log error
		lbRc = false;
	}
	return lbRc;
}
// SingleLine strings only (internal \n are allowed)
// If there were no key_value or key_value_type is unexpected, DOESN'T touch *value
bool SettingsXML::Load(const wchar_t *regName, wchar_t *value, int maxLen) noexcept
{
	_ASSERTE(maxLen>1);
	bool lbRc = false;
	wchar_t* pszValue = nullptr;

	if (Load(regName, &pszValue))
	{
		if (pszValue)
			lstrcpyn(value, pszValue, maxLen);
		else
			value[0] = 0;

		lbRc = true;
	}

	if (pszValue) free(pszValue);

	return lbRc;
}
bool SettingsXML::Load(const wchar_t *regName, LPBYTE value, DWORD nSize) noexcept
{
	bool lbRc = false;
	SettingsXML::node* pChild = nullptr;
	const char *sType = nullptr, *sData = nullptr;

	if (!value || !nSize)
		return false;

	try
	{
		if (mp_Key)
			pChild = FindItem(mp_Key, L"value", regName, false);

		if (!pChild)
			return false;

		sType = GetAttr(pChild, "type");
		sData = GetAttr(pChild, "data");
	}
	catch (rapidxml::parse_error&)
	{
		// #XML Log error
		sData = nullptr;
	}

	if (sType && sData)
	{
		lbRc = ConvertData(sType, sData, value, nSize);
	}

	return lbRc;
}

bool SettingsXML::ConvertData(const char* sType, const char* sData, LPBYTE value, DWORD nSize) noexcept
{
	bool lbRc = false;

	if (0 == _strcmpi(sType, "string"))
	{
		CEStr wc;
		if (utf2wcs(sData, wc))
		{
			#ifdef _DEBUG
			// ReSharper disable once CppDeclaratorNeverUsed
			size_t nLen = wcslen(wc);
			#endif
			const DWORD nMaxLen = nSize / sizeof(wchar_t);
			lstrcpyn(reinterpret_cast<wchar_t*>(value), wc, nMaxLen);
			lbRc = true;
		}
	} // if (0 == _strcmpi(sType, "string"))
	else if ((0 == _strcmpi(sType, "ulong")) || (0 == _strcmpi(sType, "dword")))
	{
		char* pszEnd = nullptr;
		const int radix = (0 == _strcmpi(sType, "dword")) ? 16 : 10;
		DWORD lVal = strtoul(sData, &pszEnd, radix);

		if (nSize > sizeof(lVal)) nSize = sizeof(lVal);

		if (pszEnd && pszEnd != sData)
		{
			memmove_s(value, nSize, &lVal, nSize);
			lbRc = true;
		}
	} // if ((0 == _strcmpi(sType, "ulong")) || (0 == _strcmpi(sType, "dword")))
	else if (0 == _strcmpi(sType, "long"))
	{
		char* pszEnd = nullptr;
		int lVal = strtol(sData, &pszEnd, 10);

		if (nSize > sizeof(lVal)) nSize = sizeof(lVal);

		if (pszEnd && pszEnd != sData)
		{
			memmove_s(value, nSize, &lVal, nSize);
			lbRc = true;
		}
	} // if (0 == _strcmpi(sType, "long"))
	else if (0 == _strcmpi(sType, "hex"))
	{
		LPBYTE pCur = value;
		const char* pszCur = sData;
		// ReSharper disable once CppJoinDeclarationAndAssignment
		char cHex = 0;
		// ReSharper disable once CppJoinDeclarationAndAssignment
		unsigned lVal = 0;
		unsigned digit = 0;
		lbRc = true;

		auto hexDigit = [](char cHex, unsigned& digit) -> bool
		{
			if (cHex >= '0' && cHex <= '9')
			{
				digit = cHex - '0';
			}
			else if (cHex >= 'a' && cHex <= 'f')
			{
				digit = cHex - 'a' + 10;
			}
			else if (cHex >= 'A' && cHex <= 'F')
			{
				digit = cHex - 'A' + 10;
			}
			else
			{
				return false;
			}
			return true;
		};

		while (*pszCur && nSize)
		{
			// ReSharper disable once CppAssignedValueIsNeverUsed
			lVal = 0;

			cHex = *(pszCur++);
			if (!hexDigit(cHex, digit))
			{
				lbRc = false; break;
			}
			lVal = digit;

			cHex = *pszCur;
			if (cHex && cHex != L',')
			{
				if (!hexDigit(cHex, digit))
				{
					lbRc = false; break;
				}
				lVal = (lVal << 4) | digit;
				++pszCur;
			}

			*(pCur++) = LOBYTE(lVal);
			--nSize;

			if (*pszCur != L',')
				break;
			++pszCur;
		}

		if (lbRc)
		{
			while (nSize--)  // erase value tail
				*(pCur++) = 0;
		}
	} // if (0 == _strcmpi(sType, "hex"))
	else
	{
		// don't care about other data types
	}

	return lbRc;
}

// Erase <value name="<regName>" .../>
void SettingsXML::Delete(const wchar_t *regName) noexcept
{
	DeleteImpl(regName, L"value");
}

// Erase <key name="<regName>"> ... </key>
void SettingsXML::DeleteKey(const wchar_t *regName) noexcept
{
	DeleteImpl(regName, L"key");
}

void SettingsXML::DeleteImpl(const wchar_t *regName, const wchar_t *asType) noexcept
{
	if (!mp_File || !mp_Key)
		return;

	try
	{
		node* value = FindItem(mp_Key, asType, regName, false);
		if (value)
		{
			mp_Key->remove_node(value);
		}
	}
	catch (rapidxml::parse_error&)
	{
		// #XML Log error
	}
}

bool SettingsXML::SetMultiLine(SettingsXML::node* apNode, const wchar_t* asValue, long nAllLen)
{
	bool bRc = false;
	SettingsXML::node* pNode = nullptr;
	long nLen = 0;
	const wchar_t* psz = asValue;

	pNode = apNode->first_node("line");

	// First update existing children
	while ((psz && *psz && nAllLen > 0) && (pNode))
	{
		if (!SetAttr(pNode, "data", psz))
			goto wrap;

		nLen = _tcslen(psz)+1;
		psz += nLen;
		nAllLen -= nLen;

		pNode = pNode->next_sibling("line");
	}

	// Add tail data
	while (psz && *psz && nAllLen > 0)
	{
		pNode = mp_File->allocate_node(node_element, "line");
		if (!pNode)
			goto wrap;

		if (!SetAttr(pNode, "data", psz))
			goto wrap;

		apNode->append_node(pNode);
		pNode = nullptr;
		SetDataChanged();

		nLen = _tcslen(psz)+1;
		psz += nLen;
		nAllLen -= nLen;
	}

	// Clear the tail
	if (pNode)
	{
		ClearChildrenTail(apNode, pNode, "line");
	}

	bRc = true;
wrap:
	_ASSERTE(nAllLen <= 1);
	return bRc;
}

void SettingsXML::ClearChildrenTail(SettingsXML::node* apNode, SettingsXML::node* apFirstClear, const char* nodeType)
{
	SettingsXML::node* pNext = apFirstClear;

	while (pNext)
	{
		SettingsXML::node* pDel = pNext;
		pNext = pNext->next_sibling(nodeType);
		apNode->remove_node(pDel);
		SetDataChanged();
	}
}

void SettingsXML::Save(const wchar_t *regName, LPCBYTE value, const DWORD nType, const DWORD nSize) noexcept
{
	SettingsXML::node* pChild = nullptr;
	const char* sValue = nullptr;
	const char* sType = nullptr;
	bool bNeedSetType = false;
	// nType:
	// REG_DWORD:    сохранение числа в 16-ричном или 10-чном формате, в зависимости от того, что сейчас указано в xml ("dword"/"ulong"/"long")
	// REG_BINARY:   строго в hex (FF,FF,...)
	// REG_SZ:       ASCIIZ строка, можно проконтролировать, чтобы nSize/2 не был меньше длины строки
	// REG_MULTI_SZ: ASCIIZZ. При формировании <list...> нужно убедиться, что мы не вылезли за пределы nSize

	try
	{
		pChild = FindItem(mp_Key, L"value", regName, true); // создать, если его еще нету

		if (!pChild)
			goto wrap;

		sType = GetAttr(pChild, "type");

		switch (nType)
		{
			case REG__LONG:
			case REG__ULONG:
			case REG_DWORD:
			{
				char szValue[32];

				if (sType && (sType[0] == 'u' || sType[0] == 'U'))
				{
					sprintf_c(szValue, "%u", *LPDWORD(value));
				}
				else if (sType && (sType[0] == L'l' || sType[0] == L'L'))
				{
					sprintf_c(szValue, "%i", *reinterpret_cast<const int*>(value));
				}
				else
				{
					LPCSTR pszTypeName;
					static char sDWORD[] = "dword", sULONG[] = "ulong", sLONG[] = "long";
					if (nType == REG_DWORD)
					{
						sprintf_c(szValue, "%08x", *LPDWORD(value));
						pszTypeName = sDWORD;
					}
					else if (nType == REG__ULONG)
					{
						sprintf_c(szValue, "%u", *reinterpret_cast<const UINT*>(value));
						pszTypeName = sULONG;
					}
					else
					{
						_ASSERTE(nType == REG__LONG);
						sprintf_c(szValue, "%i", *reinterpret_cast<const int*>(value));
						pszTypeName = sLONG;
					}

					if (!sType || (0 != strcmp(sType, pszTypeName)))
					{
						// add or refresh the type
						sType = pszTypeName;
						bNeedSetType = true;
					}
				}

				sValue = mp_File->allocate_string(szValue);
			} break;
			case REG_BINARY:
			{
				if (nSize == 1 && sType && (sType[0] == 'u' || sType[0] == 'U'))
				{
					char szValue[4];
					const BYTE bt = *value;
					sprintf_c(szValue, "%u", static_cast<DWORD>(bt));
					sValue = mp_File->allocate_string(szValue);
				}
				else if (nSize == 1 && sType && (sType[0] == 'l' || sType[0] == 'L'))
				{
					char szValue[4];
					const char bt = *value;
					sprintf_c(szValue, "%i", static_cast<int>(bt));
					sValue = mp_File->allocate_string(szValue);
				}
				else
				{
					// (FF,FF,...) - two char per byte + delimiting ','
					static char sHex[] = "hex";
					const DWORD nLen = nSize * 2 + (nSize - 1);
					CEStrA temp;
					char* psz = temp.GetBuffer(nLen);
					if (!psz)
						goto wrap;
					LPCBYTE  ptr = value;
					char hex[] = "0123456789ABCDEF";

					for (int i = static_cast<int>(nSize) - 1; i >= 0; --i)
					{
						const DWORD d = static_cast<DWORD>(*(ptr++));
						*(psz++) = hex[(d & 0xF0) >> 4];
						*(psz++) = hex[(d & 0x0F)];
						if (i > 0)
							*(psz++) = ',';
					}
					*psz = 0;
					sValue = mp_File->allocate_string(temp);

					if (!sType || (0 != strcmp(sType, sHex)))
					{
						// Only HEX is allowed
						bNeedSetType = true;
						sType = sHex;
					}
				}
			} break;
			case REG_SZ:
			{
				static char sSTRING[] = "string";
				const auto* psz = reinterpret_cast<const wchar_t*>(value);
				sValue = wcs2utf(psz);

				if (!sType || (0 != strcmp(sType, sSTRING)))
				{
					// Only "string" is allowed
					bNeedSetType = true;
					sType = sSTRING;
				}
			} break;
			case REG_MULTI_SZ:
			{
				static char sMSZ[] = "multi";
				if (!sType || (0 != strcmp(sType, sMSZ)))
				{
					// Only "multi" is allowed
					bNeedSetType = true;
					sType = sMSZ;
				}
			} break;
			default:
			{
				_ASSERTE(FALSE && "Unsupported REGTYPE!");
				goto wrap; // type is not supported
			}
		}

		if (bNeedSetType)
		{
			_ASSERTE(sType!=nullptr);
			SetAttr(pChild, "type", sType);
		}

		// And the value
		if (nType != REG_MULTI_SZ)
		{
			_ASSERTE(sValue != nullptr);
			SetAttr(pChild, "data", sValue);
		}
		else     // Create sequence of <line/> elements for REG_MULTI_SZ
		{
			// if there were "data" - erase the attribute
			attribute* attr_data = pChild->first_attribute("data");
			if (attr_data)
			{
				pChild->remove_attribute(attr_data);
				SetDataChanged();
			}

			const long nAllLen = static_cast<long>(nSize) / 2; // length in wchar_t
			const auto* psz = reinterpret_cast<const wchar_t*>(value);
			SetMultiLine(pChild, psz, nAllLen);
		}

		mb_Modified = true; // Save was called at least once (data may be was not changed)
	}
	catch (rapidxml::parse_error&)
	{
	}

wrap:
	return;
}
