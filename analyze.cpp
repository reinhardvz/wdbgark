/*
    * WinDBG Anti-RootKit extension
    * Copyright � 2013-2014  Vyacheslav Rusakoff
    * 
    * This program is free software: you can redistribute it and/or modify
    * it under the terms of the GNU General Public License as published by
    * the Free Software Foundation, either version 3 of the License, or
    * (at your option) any later version.
    * 
    * This program is distributed in the hope that it will be useful,
    * but WITHOUT ANY WARRANTY; without even the implied warranty of
    * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    * GNU General Public License for more details.
    * 
    * You should have received a copy of the GNU General Public License
    * along with this program.  If not, see <http://www.gnu.org/licenses/>.

    * This work is licensed under the terms of the GNU GPL, version 3.  See
    * the COPYING file in the top-level directory.
*/

#include "analyze.hpp"
#include <dbghelp.h>

#include <string>
#include <algorithm>
#include <utility>

#include "objhelper.hpp"

bool WDbgArkAnalyze::Init(std::ostream* output) {
    if ( IsInited() )
        return true;

    tp = new (nothrow) bprinter::TablePrinter(output);

    if ( tp )
        m_inited = true;

    return m_inited;
}

bool WDbgArkAnalyze::Init(std::ostream* output, const AnalyzeTypeInit type) {
    if ( IsInited() )
        return true;

    tp = new (nothrow) bprinter::TablePrinter(output);

    if ( tp ) {
        if ( type == AnalyzeTypeDefault ) {    // width = 180
            tp->AddColumn("Address", 18);
            tp->AddColumn("Name", 68);
            tp->AddColumn("Symbol", 68);
            tp->AddColumn("Module", 16);
            tp->AddColumn("Suspicious", 10);

            m_inited = true;
        } else if ( type == AnalyzeTypeCallback ) {    // width = 170
            tp->AddColumn("Address", 18);
            tp->AddColumn("Type", 20);
            tp->AddColumn("Symbol", 81);
            tp->AddColumn("Module", 16);
            tp->AddColumn("Suspicious", 10);
            tp->AddColumn("Info", 25);

            m_inited = true;
        } else if ( type == AnalyzeTypeIDT ) {    // width = 160
            tp->AddColumn("Address", 18);
            tp->AddColumn("CPU / Idx", 11);
            tp->AddColumn("Symbol", 80);
            tp->AddColumn("Module", 16);
            tp->AddColumn("Suspicious", 10);
            tp->AddColumn("Info", 25);

            m_inited = true;
        } else if ( type == AnalyzeTypeGDT ) {    // width = 105
            tp->AddColumn("Base", 18);
            tp->AddColumn("Limit", 6);
            tp->AddColumn("CPU / Idx", 10);
            tp->AddColumn("Offset", 10);
            tp->AddColumn("Selector name", 20);
            tp->AddColumn("Type", 16);
            tp->AddColumn("Info", 25);

            m_inited = true;
        }
    }

    return m_inited;
}

void WDbgArkAnalyze::AnalyzeAddressAsRoutine(const unsigned __int64 address,
                                             const std::string &type,
                                             const std::string &additional_info) {
    std::string       symbol_name;
    std::string       module_name;
    std::string       image_name;
    std::string       loaded_image_name;
    std::stringstream module_command_buf;

    if ( !IsInited() ) {
        err << __FUNCTION__ << ": class is not initialized" << endlerr;
        return;
    }

    bool suspicious = IsSuspiciousAddress(address);

    if ( address ) {
        symbol_name = "*UNKNOWN*";
        module_name = "*UNKNOWN*";

        if ( !SUCCEEDED(GetModuleNames(address, image_name, module_name, loaded_image_name)) )
            suspicious = true;

        module_command_buf << "<exec cmd=\"lmvm " << module_name << "\">" << std::setw(16) << module_name << "</exec>";

        std::pair<HRESULT, std::string> result = GetNameByOffset(address);

        if ( !SUCCEEDED(result.first) )
            suspicious = true;
        else
            symbol_name = result.second;
    }

    std::stringstream addr_ext;

    if ( address )
        addr_ext << "<exec cmd=\"u " << std::hex << std::showbase << address << " L5\">";

    addr_ext << std::internal << std::setw(18) << std::setfill('0') << std::hex << std::showbase << address;

    if ( address )
        addr_ext << "</exec>";

    *tp << addr_ext.str() << type << symbol_name << module_command_buf.str();

    if ( suspicious )
        *tp << "Y";
    else
        *tp << "";

    if ( !additional_info.empty() )
        *tp << additional_info;

    if ( suspicious )
        tp->flush_warn();
    else
        tp->flush_out();
}

void WDbgArkAnalyze::AnalyzeObjectTypeInfo(const ExtRemoteTyped &ex_type_info, const ExtRemoteTyped &object) {
    ExtRemoteTyped   obj_type_info = ex_type_info;
    std::string      object_name   = "*UNKNOWN*";
    WDbgArkObjHelper obj_helper;

    if ( !IsInited() ) {
        err << __FUNCTION__ << ": class is not initialized" << endlerr;
        return;
    }

    if ( !obj_helper.Init() )
        warn << __FUNCTION__ ": failed to init object helper class" << endlwarn;

    std::pair<HRESULT, std::string> result = obj_helper.GetObjectName(object);

    if ( !SUCCEEDED( result.first ) )
        warn << __FUNCTION__ ": GetObjectName failed" << endlwarn;
    else
        object_name = result.second;

    try {
        std::stringstream object_command;
        std::stringstream object_name_ext;

        object_command << "<exec cmd=\"!object " << std::hex << std::showbase << object.m_Offset << "\">";
        object_command << std::hex << std::showbase << object.m_Offset << "</exec>";
        object_name_ext << object_name;

        *tp << object_command.str() << object_name_ext.str();
        tp->flush_out();
        tp->PrintFooter();

        AnalyzeAddressAsRoutine(obj_type_info.Field("DumpProcedure").GetPtr(), "DumpProcedure", "");
        AnalyzeAddressAsRoutine(obj_type_info.Field("OpenProcedure").GetPtr(), "OpenProcedure", "");
        AnalyzeAddressAsRoutine(obj_type_info.Field("CloseProcedure").GetPtr(), "CloseProcedure", "");
        AnalyzeAddressAsRoutine(obj_type_info.Field("DeleteProcedure").GetPtr(), "DeleteProcedure", "");
        AnalyzeAddressAsRoutine(obj_type_info.Field("ParseProcedure").GetPtr(), "ParseProcedure", "");
        AnalyzeAddressAsRoutine(obj_type_info.Field("SecurityProcedure").GetPtr(), "SecurityProcedure", "");
        AnalyzeAddressAsRoutine(obj_type_info.Field("SecurityProcedure").GetPtr(), "QueryNameProcedure", "");
        tp->PrintFooter();
    }
    catch( const ExtRemoteException &Ex ) {
        err << __FUNCTION__ << ": " << Ex.GetMessage() << endlerr;
    }
}

void WDbgArkAnalyze::AnalyzeGDTEntry(const ExtRemoteTyped &gdt_entry,
                                     const std::string &cpu_idx,
                                     const unsigned __int32 selector,
                                     const std::string &additional_info) {
    ExtRemoteTyped loc_gdt_entry = gdt_entry;

    if ( !IsInited() ) {
        err << __FUNCTION__ << ": class is not initialized" << endlerr;
        return;
    }

    try {
        unsigned __int64 address = 0;
        unsigned __int32 limit   = 0;

        if ( g_Ext->IsCurMachine64() ) {
            if ( selector != KGDT64_R0_DATA && selector != KGDT64_R3_DATA ) {
                address =\
                    (static_cast<unsigned __int64>(loc_gdt_entry.Field("BaseLow").GetUshort())) |\
                    (static_cast<unsigned __int64>(loc_gdt_entry.Field("Bytes.BaseMiddle").GetUchar()) << 16) |\
                    (static_cast<unsigned __int64>(loc_gdt_entry.Field("Bytes.BaseHigh").GetUchar()) << 24) |\
                    (static_cast<unsigned __int64>(loc_gdt_entry.Field("BaseUpper").GetUlong()) << 32);

                limit =\
                    (loc_gdt_entry.Field("LimitLow").GetUshort()) |\
                    (loc_gdt_entry.Field("Bits").GetUlong() & 0x0F0000);
            }
        } else {
            address =\
                (static_cast<unsigned __int64>(loc_gdt_entry.Field("BaseLow").GetUshort())) |\
                (static_cast<unsigned __int64>(loc_gdt_entry.Field("HighWord.Bytes.BaseMid").GetUchar()) << 16) |\
                (static_cast<unsigned __int64>(loc_gdt_entry.Field("HighWord.Bytes.BaseHi").GetUchar()) << 24);

            limit =\
                (loc_gdt_entry.Field("LimitLow").GetUshort()) |\
                (loc_gdt_entry.Field("HighWord.Bits").GetUlong() & 0x0F0000);
        }

        std::stringstream expression;
        expression << std::hex << std::showbase <<  address;
        address = g_Ext->EvalExprU64(expression.str().c_str());

        std::stringstream addr_ext;

        if ( address ) {
            if ( g_Ext->IsCurMachine64() ) {
                if ( selector == KGDT64_SYS_TSS )
                    addr_ext << "<exec cmd=\"dt nt!_KTSS64 " << std::hex << std::showbase << address << "\">";
            } else {
                if ( selector == KGDT_TSS || selector == KGDT_DF_TSS || selector == KGDT_NMI_TSS )
                    addr_ext << "<exec cmd=\"dt nt!_KTSS " << std::hex << std::showbase << address << "\">";
                else if ( selector == KGDT_R0_PCR )
                    addr_ext << "<exec cmd=\"dt nt!_KPCR " << std::hex << std::showbase << address << "\">";
            }
        }

        addr_ext << std::internal << std::setw(18) << std::setfill('0') << std::hex << std::showbase << address;

        if ( address ) {
            if ( g_Ext->IsCurMachine64() ) {
                if ( selector == KGDT64_SYS_TSS )
                    addr_ext << "</exec>";
            } else {
                if ( selector == KGDT_TSS || selector == KGDT_DF_TSS || selector == KGDT_NMI_TSS || selector == KGDT_R0_PCR )
                    addr_ext << "</exec>";
            }
        }

        std::stringstream selector_ext;
        selector_ext << std::hex << std::showbase << selector;

        *tp << addr_ext.str() << limit << cpu_idx << selector_ext.str() << GetGDTSelectorName(selector) << "type" << additional_info;
        tp->flush_out();
    }
    catch( const ExtRemoteException &Ex ) {
        err << __FUNCTION__ << ": " << Ex.GetMessage() << endlerr;
    }
}

std::string WDbgArkAnalyze::GetGDTSelectorName(const unsigned __int32 selector) const {
    if ( g_Ext->IsCurMachine64() ) {
        switch ( selector ) {
            case KGDT64_NULL:
                return make_string( KGDT64_NULL );

            case KGDT64_R0_CODE:
                return make_string( KGDT64_R0_CODE );

            case KGDT64_R0_DATA:
                return make_string( KGDT64_R0_DATA );

            case KGDT64_R3_CMCODE:
                return make_string( KGDT64_R3_CMCODE );

            case KGDT64_R3_DATA:
                return make_string( KGDT64_R3_DATA );

            case KGDT64_R3_CODE:
                return make_string( KGDT64_R3_CODE );

            case KGDT64_SYS_TSS:
                return make_string( KGDT64_SYS_TSS );

            case KGDT64_R3_CMTEB:
                return make_string( KGDT64_R3_CMTEB );

            default:
                return "*UNKNOWN*";
        }
    } else {
        switch ( selector ) {
            case KGDT_R0_CODE:
                return make_string( KGDT_R0_CODE );

            case KGDT_R0_DATA:
                return make_string( KGDT_R0_DATA );

            case KGDT_R3_CODE:
                return make_string( KGDT_R3_CODE );

            case KGDT_R3_DATA:
                return make_string( KGDT_R3_DATA );

            case KGDT_TSS:
                return make_string( KGDT_TSS );

            case KGDT_R0_PCR:
                return make_string( KGDT_R0_PCR );

            case KGDT_R3_TEB:
                return make_string( KGDT_R3_TEB );

            case KGDT_LDT:
                return make_string( KGDT_LDT );

            case KGDT_DF_TSS:
                return make_string( KGDT_DF_TSS );

            case KGDT_NMI_TSS:
                return make_string( KGDT_NMI_TSS );

            case KGDT_GDT_ALIAS:
                return make_string( KGDT_GDT_ALIAS );

            case KGDT_CDA16:
                return make_string( KGDT_CDA16 );

            case KGDT_CODE16:
                return make_string( KGDT_CODE16 );

            case KGDT_STACK16:
                return make_string( KGDT_STACK16 );

            default:
                return "*UNKNOWN*";
        }
    }
}

HRESULT WDbgArkAnalyze::GetModuleNames(const unsigned __int64 address,
                                       std::string &image_name,
                                       std::string &module_name,
                                       std::string &loaded_image_name) {
    unsigned __int32  len1, len2, len3 = 0;
    unsigned __int64  module_base      = 0;
    unsigned __int32  module_index     = 0;
    ExtCaptureOutputA ignore_output;

    if ( !address )
        return E_INVALIDARG;

    ignore_output.Start();

    HRESULT result = g_Ext->m_Symbols->GetModuleByOffset(address, 0, reinterpret_cast<PULONG>(&module_index), &module_base);

    if ( SUCCEEDED(result) ) {
        result = g_Ext->m_Symbols->GetModuleNames(module_index,
                                                  module_base,
                                                  NULL,
                                                  0,
                                                  reinterpret_cast<PULONG>(&len1),
                                                  NULL,
                                                  0,
                                                  reinterpret_cast<PULONG>(&len2),
                                                  NULL,
                                                  0,
                                                  reinterpret_cast<PULONG>(&len3));

        if ( SUCCEEDED(result) ) {
            char* buf1 = new (nothrow) char[ len1 + 1 ];
            char* buf2 = new (nothrow) char[ len2 + 1 ];
            char* buf3 = new (nothrow) char[ len3 + 1 ];

            if ( !buf1 || !buf2 || !buf3 ) {
                if ( buf3 )
                    delete[] buf3;

                if ( buf2 )
                    delete[] buf2;

                if ( buf1 )
                    delete[] buf1;

                ignore_output.Stop();
                return E_OUTOFMEMORY;
            }

            ZeroMemory(buf1, len1 + 1);
            ZeroMemory(buf2, len2 + 1);
            ZeroMemory(buf3, len3 + 1);

            result = g_Ext->m_Symbols->GetModuleNames(module_index,
                                                      module_base,
                                                      buf1,
                                                      len1 + 1,
                                                      NULL,
                                                      buf2,
                                                      len2 + 1,
                                                      NULL,
                                                      buf3,
                                                      len3 + 1,
                                                      NULL);

            if ( SUCCEEDED(result) ) {
                image_name = buf1;
                transform(image_name.begin(), image_name.end(), image_name.begin(), tolower);

                module_name = buf2;
                transform(module_name.begin(), module_name.end(), module_name.begin(), tolower);

                loaded_image_name = buf3;
                transform(loaded_image_name.begin(), loaded_image_name.end(), loaded_image_name.begin(), tolower);
            }

            delete[] buf3;
            delete[] buf2;
            delete[] buf1;
        }
    }

    ignore_output.Stop();
    return result;
}

std::pair<HRESULT, std::string> WDbgArkAnalyze::GetNameByOffset(const unsigned __int64 address) {
    string            output_name      = "";
    unsigned __int32  name_buffer_size = 0;
    unsigned __int64  displacement     = 0;
    std::stringstream stream_name;
    ExtCaptureOutputA ignore_output;

    if ( !address )
        return make_pair(E_INVALIDARG, output_name);

    ignore_output.Start();
    HRESULT result = g_Ext->m_Symbols->GetNameByOffset(address,
                                                       NULL,
                                                       0,
                                                       reinterpret_cast<PULONG>(&name_buffer_size),
                                                       &displacement);
    ignore_output.Stop();

    if ( SUCCEEDED(result) && name_buffer_size ) {
        char* tmp_name = new (nothrow) char[ name_buffer_size + 1 ];

        if ( !tmp_name )
            return make_pair(E_OUTOFMEMORY, output_name);

        ZeroMemory(tmp_name, name_buffer_size + 1);

        ignore_output.Start();
        result = g_Ext->m_Symbols->GetNameByOffset(address, tmp_name, name_buffer_size, NULL, NULL);
        ignore_output.Stop();

        if ( SUCCEEDED(result) ) {
            stream_name << tmp_name;

            if ( displacement )
                stream_name << "+" << std::hex << std::showbase << displacement;

            output_name = stream_name.str();
        }

        delete[] tmp_name;
    }

    return make_pair(result, output_name);
}

bool WDbgArkAnalyze::SetOwnerModule(const std::string &module_name) {
    if ( !IsInited() ) {
        err << __FUNCTION__ << ": class is not initialized" << endlerr;
        return false;
    }

    try {
        const unsigned __int32 index = g_Ext->FindFirstModule(module_name.c_str(), NULL, 0);

        if ( SUCCEEDED(g_Ext->m_Symbols->GetModuleByIndex(index, &m_owner_module_start)) ) {
            IMAGEHLP_MODULEW64 info;
            g_Ext->GetModuleImagehlpInfo(m_owner_module_start, &info);

            m_owner_module_end = m_owner_module_start + info.ImageSize;
            m_owner_module_inited = true;

            return true;
        }
    }
    catch ( const ExtStatusException &Ex ) {
        err << __FUNCTION__ << ": " << Ex.GetMessage() << endlerr;
    }

    return false;
}

bool WDbgArkAnalyze::IsSuspiciousAddress(const unsigned __int64 address) {
    if ( !m_owner_module_inited )
        return false;

    if ( !address )
        return false;

    if ( address >= m_owner_module_start && address <= m_owner_module_end )
        return false;

    return true;
}