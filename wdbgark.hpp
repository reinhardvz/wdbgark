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

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef WDBGARK_HPP_
#define WDBGARK_HPP_

#if defined( _DEBUG )
    #define _CRTDBG_MAP_ALLOC
    #include <stdlib.h>
    #include <crtdbg.h>
#endif // _DEBUG

#include <string>
#include <sstream>
#include <map>
#include <vector>
using namespace std;

#undef EXT_CLASS
#define EXT_CLASS WDbgArk
#include <engextcpp.hpp>

#include "sdt_w32p.hpp"
#include "objhelper.hpp"

//////////////////////////////////////////////////////////////////////////
// main class
//////////////////////////////////////////////////////////////////////////
class WDbgArk : public ExtExtension
{
 public:
    //////////////////////////////////////////////////////////////////////////
    // class typedefs
    //////////////////////////////////////////////////////////////////////////
    typedef struct SystemCbCommandTag
    {
        std::string      list_count_name;
        std::string      list_head_name;
        unsigned __int32 offset_to_routine;
    } SystemCbCommand;

    //////////////////////////////////////////////////////////////////////////
    typedef struct OutputWalkInfoTag
    {
        unsigned __int64 routine_address;
        std::string      type;
        std::string      info;
        std::string      list_head_name;
        unsigned __int64 object_offset;
        unsigned __int64 list_head_offset;
    } OutputWalkInfo;

    typedef std::vector<OutputWalkInfo> walkresType;

    typedef struct WalkCallbackContextTag
    {
        std::string      type;
        std::string      list_head_name;
        walkresType*     output_list_pointer;
        unsigned __int64 list_head_offset;
    } WalkCallbackContext;
    //////////////////////////////////////////////////////////////////////////
    typedef HRESULT (*pfn_object_directory_walk_callback_routine)(WDbgArk* wdbg_ark_class,
                                                                  const ExtRemoteTyped &object,
                                                                  void* context);

    typedef HRESULT (*pfn_any_list_w_pobject_walk_callback_routine)(WDbgArk* wdbg_ark_class,
                                                                    ExtRemoteData &object_pointer,
                                                                    void* context);

    typedef HRESULT (*pfn_device_node_walk_callback_routine)(WDbgArk* wdbg_ark_class,
                                                             ExtRemoteTyped &device_node,
                                                             void* context);

    //////////////////////////////////////////////////////////////////////////
    // main commands
    //////////////////////////////////////////////////////////////////////////
    WDbgArk() :
        m_inited(false),
        m_is_cur_machine64(false),
        m_platform_id(0),
        m_major_build(0),
        m_minor_build(0),
        m_service_pack_number(0) {

#if defined(_DBG)
        _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
        _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_DEBUG );
        //_CrtSetBreakAlloc( 143 );
#endif // _DBG
        
    }

    ~WDbgArk() {
        system_cb_commands.clear();
        callout_names.clear();
        gdt_selectors.clear();

#if defined(_DBG)
        _CrtDumpMemoryLeaks();
#endif // _DBG

    }

    EXT_COMMAND_METHOD(wa_ver);
    EXT_COMMAND_METHOD(wa_scan);
    EXT_COMMAND_METHOD(wa_systemcb);
    EXT_COMMAND_METHOD(wa_objtype);
    EXT_COMMAND_METHOD(wa_objtypeidx);
    EXT_COMMAND_METHOD(wa_callouts);
    EXT_COMMAND_METHOD(wa_pnptable);
    EXT_COMMAND_METHOD(wa_ssdt);
    EXT_COMMAND_METHOD(wa_w32psdt);
    EXT_COMMAND_METHOD(wa_checkmsr);
    EXT_COMMAND_METHOD(wa_idt);
    EXT_COMMAND_METHOD(wa_gdt);

    //////////////////////////////////////////////////////////////////////////
    // init
    //////////////////////////////////////////////////////////////////////////
    bool Init(void);
    bool IsInited(void) const { return m_inited; }
    bool IsLiveKernel(void) const { return m_DebuggeeClass == DEBUG_KERNEL_CONNECTION; }

    void RequireLiveKernelMode(void) const throw(...) {
        if ( !IsLiveKernel() ) { throw ExtStatusException( S_OK, "live kernel-mode only" ); }
    }

    //////////////////////////////////////////////////////////////////////////
    // walk routines
    //////////////////////////////////////////////////////////////////////////
    void CallCorrespondingWalkListRoutine(const std::map<string, SystemCbCommand>::const_iterator &citer,
                                          walkresType &output_list);

    void WalkExCallbackList(const std::string &list_count_name,
                            const std::string &list_head_name,
                            const std::string &type,
                            walkresType &output_list);

    void WalkAnyListWithOffsetToRoutine(const std::string &list_head_name,
                                        const unsigned __int64 offset_list_head,
                                        const unsigned __int32 link_offset,
                                        const bool is_double,
                                        const unsigned __int32 offset_to_routine,
                                        const std::string &type,
                                        const std::string &ext_info,
                                        walkresType &output_list);

    void WalkAnyListWithOffsetToObjectPointer(const std::string &list_head_name,
                                              const unsigned __int64 offset_list_head,
                                              const bool is_double,
                                              const unsigned __int32 offset_to_object_pointer,
                                              void* context,
                                              pfn_any_list_w_pobject_walk_callback_routine callback);

    void WalkDeviceNode(const unsigned __int64 device_node_address,
                        void* context,
                        pfn_device_node_walk_callback_routine callback);

    void WalkShutdownList(const std::string &list_head_name, const std::string &type, walkresType &output_list);
    void WalkPnpLists(const std::string &type, walkresType &output_list);
    void WalkCallbackDirectory(const std::string &type, walkresType &output_list);

    void AddSymbolPointer(const std::string &symbol_name,
                          const std::string &type,
                          const std::string &additional_info,
                          walkresType &output_list);

    void WalkDirectoryObject(const unsigned __int64 directory_address,
                             void* context,
                             pfn_object_directory_walk_callback_routine callback);

 private:
    std::map<string, SystemCbCommand> system_cb_commands;
    std::vector<string>               callout_names;
    std::vector<unsigned __int32>     gdt_selectors;

    //////////////////////////////////////////////////////////////////////////
    // callback routines
    //////////////////////////////////////////////////////////////////////////
    static HRESULT DirectoryObjectCallback(WDbgArk* wdbg_ark_class,
                                           const ExtRemoteTyped &object,
                                           void* context);

    static HRESULT ShutdownListCallback(WDbgArk* wdbg_ark_class,
                                        ExtRemoteData &object_pointer,
                                        void* context);

    static HRESULT DirectoryObjectTypeCallback(WDbgArk* wdbg_ark_class,
                                               const ExtRemoteTyped &object,
                                               void* context);

    static HRESULT DeviceNodeCallback(WDbgArk* wdbg_ark_class,
                                      ExtRemoteTyped &device_node,
                                      void* context);

    //////////////////////////////////////////////////////////////////////////
    // helpers
    //////////////////////////////////////////////////////////////////////////
    unsigned __int32 GetCmCallbackItemFunctionOffset();
    unsigned __int32 GetPowerCallbackItemFunctionOffset();
    unsigned __int32 GetPnpCallbackItemFunctionOffset();

    std::string get_service_table_routine_name_internal(const unsigned __int32 index,
                                                        const unsigned __int32 max_count,
                                                        const char** service_table) const;

    std::string get_service_table_routine_name(const ServiceTableType type, const unsigned __int32 index) const;
    std::string get_service_table_prefix_name(const ServiceTableType type) const;

    //////////////////////////////////////////////////////////////////////////
    // variables
    //////////////////////////////////////////////////////////////////////////
    bool             m_inited;
    bool             m_is_cur_machine64;
    unsigned __int32 m_platform_id;
    unsigned __int32 m_major_build;
    unsigned __int32 m_minor_build;
    unsigned __int32 m_service_pack_number;

    WDbgArkObjHelper m_obj_helper;

    //////////////////////////////////////////////////////////////////////////
    // output streams
    //////////////////////////////////////////////////////////////////////////
    std::stringstream out;
    std::stringstream warn;
    std::stringstream err;
};

#endif // WDBGARK_HPP_