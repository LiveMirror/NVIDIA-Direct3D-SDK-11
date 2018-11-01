#include "PrecompiledHeader.h"
#include "D3DHelpers.h"

std::wstring ToWideString(const char* pStr)
{
    int len = strlen(pStr);

    //ASSERT_PTR( pStr ) ; 
    //ASSERT( len >= 0 || len == -1 , _T("Invalid string length: ") << len ) ; 

    // figure out how many wide characters we are going to get 

    int nChars = MultiByteToWideChar( CP_ACP , 0 , pStr , len , NULL , 0 ) ; 
    if ( len == -1 )
        -- nChars ; 
    if ( nChars == 0 )
        return L"" ;

    // convert the narrow string to a wide string 
    // nb: slightly naughty to write directly into the string like this
    std::wstring buf ;
    buf.resize( nChars ) ; 
    MultiByteToWideChar( CP_ACP , 0 , pStr , len , 
        const_cast<wchar_t*>(buf.c_str()) , nChars ) ; 

    return buf ;
}


std::string ToNarrowString(const wchar_t* pStr)
{
    int len = wcslen(pStr);
    //ASSERT( pStr!=0 ) ; 
    //ASSERT( len >= 0 || len == -1 , _T("Invalid string length: ") << len ) ; 

    // figure out how many narrow characters we are going to get 

    int nChars = WideCharToMultiByte( CP_ACP , 0 , 
        pStr , len , NULL , 0 , NULL , NULL ) ; 
    if ( len == -1 )
        -- nChars ; 
    if ( nChars == 0 )
        return "" ;

    // convert the wide string to a narrow string

    // nb: slightly naughty to write directly into the string like this

    std::string buf ;
    buf.resize( nChars ) ;
    WideCharToMultiByte( CP_ACP , 0 , pStr , len , 
        const_cast<char*>(buf.c_str()) , nChars , NULL , NULL ) ; 

    return buf ; 
}

void ThrowStdRuntimeError(const char* file, int line, const char* err_msg)
{
    char lineStr[64];

    _itoa(line,lineStr,10);

    throw std::runtime_error( std::string("file: ") + std::string(file) +
        std::string(", line: ") + lineStr + std::string(": ") + err_msg);
}

/*static void ThrowStdRuntimeError(const wchar_t* file, int line, const wchar_t* err_msg)
{
    char lineStr[64];
    _itoa(line,lineStr,10);

    std::string fileName = ToNarrowString(file);
    std::string errMsg = ToNarrowString(file);

    throw std::runtime_error( std::string("file: ") + fileName +
        std::string(", line: ") + lineStr + std::string(": ") + errMsg);
}*/

void ThrowStdRuntimeErrorU(const wchar_t* file, int line, const wchar_t* err_msg)
{
    char lineStr[64];
    _itoa(line,lineStr,10);

    std::string fileName = ToNarrowString(file);
    std::string errMsg = ToNarrowString(err_msg);

    throw std::runtime_error( std::string("file: ") + fileName +
        std::string(", line: ") + lineStr + std::string(": ") + errMsg);
}  
