/*
 * Copyright 2013 Sveriges Television AB http://casparcg.com/
 *
 * This file is part of CasparCG (www.casparcg.com).
 *
 * CasparCG is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CasparCG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Julian Waller, git@julusian.co.uk
 */

#include "../StdAfx.h"

#include "ndi_instance.h"

#include <memory>

#include <common/except.h>
#include <mutex>

namespace caspar { namespace newtek { namespace ndi {

std::shared_ptr<const NDIlib_v3> load_library()
{
#ifdef _WIN32
    // We check whether the NDI run-time is installed
	const char* p_ndi_runtime_v3 = getenv(NDILIB_REDIST_FOLDER);
	if (!p_ndi_runtime_v3)
	    CASPAR_THROW_EXCEPTION(caspar_exception() << msg_info(L"Could not find the NDI Runtime. Check the README for installation instructions"));
	}

	// We now load the DLL as it is installed
	std::string ndi_path = p_ndi_runtime_v3;
	ndi_path += "\\" NDILIB_LIBRARY_NAME;

	// Try to load the library
	HMODULE hNDILib = LoadLibraryA(ndi_path.c_str());

	// The main NDI entry point for dynamic loading if we got the librari
	const NDIlib_v3* (*NDIlib_v3_load)(void) = NULL;
	if (hNDILib)
		*((FARPROC*)&NDIlib_v3_load) = GetProcAddress(hNDILib, "NDIlib_v3_load");

	// If we failed to load the library then we tell people to re-install it
	if (!NDIlib_v3_load)
	{	// Unload the DLL if we loaded it
		if (hNDILib)
			FreeLibrary(hNDILib);

	    CASPAR_THROW_EXCEPTION(caspar_exception() << msg_info(L"The NDI Runtime is not correctly installed. Please re-install"));
	}
#else
    std::string ndi_path;

    const char* p_NDI_runtime_folder = getenv(NDILIB_REDIST_FOLDER);
    if (p_NDI_runtime_folder)
    {
        ndi_path = p_NDI_runtime_folder;
        ndi_path += NDILIB_LIBRARY_NAME;
    }
    else ndi_path = NDILIB_LIBRARY_NAME;

    // Try to load the library
    void *hNDILib = dlopen(ndi_path.c_str(), RTLD_LOCAL | RTLD_LAZY);

    // The main NDI entry point for dynamic loading if we got the library
    const NDIlib_v3* (*NDIlib_v3_load)(void) = NULL;
    if (hNDILib)
        *((void**)&NDIlib_v3_load) = dlsym(hNDILib, "NDIlib_v3_load");

    // If we failed to load the library then we tell people to re-install it
    if (!NDIlib_v3_load)
    {	// Unload the library if we loaded it
        if (hNDILib)
            dlclose(hNDILib);

        CASPAR_THROW_EXCEPTION(caspar_exception() << msg_info(L"The NDI Runtime is not correctly installed. Please re-install"));
    }
#endif

    // Lets get all of the DLL entry points
    const NDIlib_v3* p_NDILib = NDIlib_v3_load();

    // We can now run as usual
    if (!p_NDILib->NDIlib_initialize())
    {
        p_NDILib->NDIlib_destroy();
        CASPAR_THROW_EXCEPTION(caspar_exception() << msg_info(L"The NDI Runtime failed to initialise"));
    }

    return std::shared_ptr<const NDIlib_v3>(p_NDILib, [](const NDIlib_v3* p_NDILib){
        p_NDILib->NDIlib_destroy();

        // Free the NDI Library
//#if _WIN32
//        FreeLibrary(hNDILib);
//#else
//        dlclose(hNDILib);
//#endif
    });
}

const std::shared_ptr<const NDIlib_v3> get_instance(){
    static std::mutex mutex;

    std::lock_guard<std::mutex> lock(mutex);

    static std::shared_ptr<const NDIlib_v3> lib = load_library();

    return lib;
}

}}} // namespace caspar::newtek::airsend
