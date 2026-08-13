# Dump every plugin-facing symbol from the runtime static libs into a PE .def.
# Editor.exe cannot --export-all-symbols (PE max 65535; the full link overflows).
# These four archives are the gameplay ABI (frigga/freyr/skirnir/simdjson); new
# fg::/fr::/skr:: APIs are exported automatically — no namespace allowlist.
#
# T = code, R/D/B/S/C = data (vtables, typeinfo, simdjson tables). Data must be
# marked DATA or the plugin import thunk points at a JMP, not the object.
# libstdc++ / Itanium std instantiations stay in the plugin's own CRT.
#
# Expected -D: NM, OUT, LIB0..LIB7 (optional missing ok)

if(NOT NM)
    set(NM nm)
endif()
if(NOT OUT)
    message(FATAL_ERROR "GeneratePluginExports: OUT is required")
endif()

set(_libs "")
foreach(_i RANGE 0 7)
    if(DEFINED LIB${_i} AND EXISTS "${LIB${_i}}")
        list(APPEND _libs "${LIB${_i}}")
    endif()
endforeach()
if(_libs STREQUAL "")
    message(FATAL_ERROR "GeneratePluginExports: no archive libraries found")
endif()

set(_std_or_crt
    "^\\.weak|^_ZSt|^_ZNSt|^_ZNKSt|^_ZNSa|^_ZNKSa|^_ZN9__gnu_cxx|^_ZTVNSt|^_ZTINSt|^_ZTSNSt|^_ZZNSt|^_ZGVNSt|^_Unwind|^__imp_|^__gnu_lto")

set(_code_syms "")
set(_data_syms "")
foreach(_lib IN LISTS _libs)
    execute_process(
        COMMAND "${NM}" -g --defined-only "${_lib}"
        OUTPUT_VARIABLE _nm
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REPLACE "\r" "" _nm "${_nm}")
    string(REPLACE "\n" ";" _lines "${_nm}")
    foreach(_line IN LISTS _lines)
        if(_line MATCHES " ([TDRBSC]) ([A-Za-z_][A-Za-z0-9_]*)$")
            set(_kind "${CMAKE_MATCH_1}")
            set(_sym "${CMAKE_MATCH_2}")
        else()
            continue()
        endif()
        if(_sym MATCHES "${_std_or_crt}")
            continue()
        endif()
        if(_kind STREQUAL "T")
            list(APPEND _code_syms "${_sym}")
        else()
            list(APPEND _data_syms "${_sym}")
        endif()
    endforeach()
endforeach()

list(REMOVE_DUPLICATES _code_syms)
list(REMOVE_DUPLICATES _data_syms)
list(SORT _code_syms)
list(SORT _data_syms)
list(LENGTH _code_syms _n_code)
list(LENGTH _data_syms _n_data)
math(EXPR _count "${_n_code} + ${_n_data}")
if(_count EQUAL 0)
    message(FATAL_ERROR "GeneratePluginExports: no matching symbols")
endif()
if(_count GREATER 65000)
    message(FATAL_ERROR "GeneratePluginExports: ${_count} symbols exceeds PE export limit")
endif()

set(_body "EXPORTS\n")
if(_n_code GREATER 0)
    list(JOIN _code_syms "\n    " _code_body)
    string(APPEND _body "    ${_code_body}\n")
endif()
if(_n_data GREATER 0)
    list(JOIN _data_syms " DATA\n    " _data_body)
    string(APPEND _body "    ${_data_body} DATA\n")
endif()
file(WRITE "${OUT}" "${_body}")
message(STATUS "GeneratePluginExports: wrote ${_count} symbols (${_n_code} code, ${_n_data} data) to ${OUT}")
