/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PluginPackage.h"

#include "MIMETypeRegistry.h"
#include "npinterface.h"
#include "npruntime_impl.h"
#include "PluginDatabase.h"
#include "PluginDebug.h"
#include <QPluginLoader>
#include <wtf/text/CString.h>


namespace WebCore {

#if ENABLE(NETSCAPE_PLUGIN_API)
bool PluginPackage::fetchInfo()
{
    if (!load())
        return false;

    char* buf = 0;
    NPError err = m_pluginFuncs.getvalue(0, NPPVpluginNameString, (void *)&buf);
    m_name = buf;
    err = m_pluginFuncs.getvalue(0, NPPVpluginDescriptionString, (void *)&buf);
    m_description = buf;

    determineModuleVersionFromDescription();

    String s = m_npInterface->NP_GetMIMEDescription();
    Vector<String> types;
    s.split(UChar('|'), false, types);  // <MIME1>;<ext1,ext2,ext3,...>;<Description>|<MIME2>|<MIME3>|...

    for (int i = 0; i < types.size(); ++i) {
        Vector<String> mime;
        types[i].split(UChar(';'), true, mime);  // <MIME1>;<ext1,ext2,ext3,...>;<Description>
        if (mime.size() > 0) {
            Vector<String> exts;
            if (mime.size() > 1)
                mime[1].split(UChar(','), false, exts); // <ext1,ext2,ext3,...>
            
            m_mimeToExtensions.add(mime[0], exts); // <MIME>,<ext1,ext2,ext3>
            determineQuirks(mime[0]);
            if (mime.size() > 2)
                m_mimeToDescriptions.add(mime[0], mime[2]); // <MIME>,<Description>
        }
    }
    unload();
    return true;
}

#if 0
void PluginPackage::determineQuirks(const String& mimeType)
{
    if (mimeType == "application/x-shockwave-flash") {
        PlatformModuleVersion flashTenVersion(0x000a0000);
        if (compareFileVersion(flashTenVersion) >= 0) {
            // Flash 10 doesn't like having a 0 window handle.
            m_quirks.add(PluginQuirkDontSetNullWindowHandleOnDestroy);
        }
    }
}
#endif

bool PluginPackage::load()
{
    if (m_isLoaded) {
        m_loadCount++;
        return true;
    }

    m_pluginLoader = new QPluginLoader(m_path);
    if (!m_pluginLoader->load()) {
        delete m_pluginLoader;
        m_pluginLoader = 0;
        return false;
    }

    QObject* plugin = m_pluginLoader->instance();
    if (!plugin) {
        m_pluginLoader->unload();
        delete m_pluginLoader;
        m_pluginLoader = 0;
        return false;
    }

    // Plugin instance created
    // Cast plugin to NPInterface,
    m_npInterface = qobject_cast<NPInterface*>(plugin);
    if (!m_npInterface) {
        m_pluginLoader->unload();
        delete m_pluginLoader;
        m_pluginLoader = 0;
        return false;
    }

    m_isLoaded = true;
    
    NPError npErr;
    memset(&m_pluginFuncs, 0, sizeof(m_pluginFuncs));
    m_pluginFuncs.size = sizeof(m_pluginFuncs);
    m_browserFuncs.size = sizeof(m_browserFuncs);
    m_browserFuncs.version = NP_VERSION_MINOR;
    m_browserFuncs.geturl = NPN_GetURL;
    m_browserFuncs.posturl = NPN_PostURL;
    m_browserFuncs.requestread = NPN_RequestRead;
    m_browserFuncs.newstream = NPN_NewStream;
    m_browserFuncs.write = NPN_Write;
    m_browserFuncs.destroystream = NPN_DestroyStream;
    m_browserFuncs.status = NPN_Status;
    m_browserFuncs.uagent = NPN_UserAgent;
    m_browserFuncs.memalloc = NPN_MemAlloc;
    m_browserFuncs.memfree = NPN_MemFree;
    m_browserFuncs.memflush = NPN_MemFlush;
    m_browserFuncs.reloadplugins = NPN_ReloadPlugins;
    m_browserFuncs.geturlnotify = NPN_GetURLNotify;
    m_browserFuncs.posturlnotify = NPN_PostURLNotify;
    m_browserFuncs.getvalue = NPN_GetValue;
    m_browserFuncs.setvalue = NPN_SetValue;
    m_browserFuncs.invalidaterect = NPN_InvalidateRect;
    m_browserFuncs.invalidateregion = NPN_InvalidateRegion;
    m_browserFuncs.forceredraw = NPN_ForceRedraw;
    m_browserFuncs.getJavaEnv = NPN_GetJavaEnv;
    m_browserFuncs.getJavaPeer = NPN_GetJavaPeer;
    m_browserFuncs.pushpopupsenabledstate = NPN_PushPopupsEnabledState;
    m_browserFuncs.poppopupsenabledstate = NPN_PopPopupsEnabledState;
    m_browserFuncs.releasevariantvalue = _NPN_ReleaseVariantValue;
    m_browserFuncs.getstringidentifier = _NPN_GetStringIdentifier;
    m_browserFuncs.getstringidentifiers = _NPN_GetStringIdentifiers;
    m_browserFuncs.getintidentifier = _NPN_GetIntIdentifier;
    m_browserFuncs.identifierisstring = _NPN_IdentifierIsString;
    m_browserFuncs.utf8fromidentifier = _NPN_UTF8FromIdentifier;
    m_browserFuncs.createobject = _NPN_CreateObject;
    m_browserFuncs.retainobject = _NPN_RetainObject;
    m_browserFuncs.releaseobject = _NPN_ReleaseObject;
    m_browserFuncs.invoke = _NPN_Invoke;
    m_browserFuncs.invokeDefault = _NPN_InvokeDefault;
    m_browserFuncs.evaluate = _NPN_Evaluate;
    m_browserFuncs.getproperty = _NPN_GetProperty;
    m_browserFuncs.setproperty = _NPN_SetProperty;
    m_browserFuncs.removeproperty = _NPN_RemoveProperty;
    m_browserFuncs.hasproperty = _NPN_HasMethod;
    m_browserFuncs.hasmethod = _NPN_HasProperty;
    m_browserFuncs.setexception = _NPN_SetException;
    m_browserFuncs.enumerate = _NPN_Enumerate;
    m_browserFuncs.construct = _NPN_Construct;

    npErr = m_npInterface->NP_Initialize(&m_browserFuncs, &m_pluginFuncs);
    if (npErr != NPERR_NO_ERROR) {
        m_pluginLoader->unload();
        delete m_pluginLoader;
        m_pluginLoader = 0;
        return false;
    }

    m_loadCount++;
    return true;
}
#endif

void PluginPackage::unload()
{
    if (!m_isLoaded)
        return;

    if (--m_loadCount > 0)
        return;
        
    m_isLoaded = false;
    m_npInterface->NP_Shutdown();

    m_pluginLoader->unload();
    delete m_pluginLoader;
    m_pluginLoader = 0;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
uint16_t PluginPackage::NPVersion() const
{
    return NP_VERSION_MINOR;
}


void PluginPackage::setMIMEDescription(const String& mimeDescription)
{
/*
    m_fullMIMEDescription = mimeDescription.lower();

    Vector<String> types;
    mimeDescription.lower().split(UChar(';'), false, types);
    for (unsigned i = 0; i < types.size(); ++i) {
        Vector<String> mime;
        types[i].split(UChar(':'), true, mime);
        if (mime.size() > 0) {
            Vector<String> exts;
            if (mime.size() > 1)
                mime[1].split(UChar(','), false, exts);
            determineQuirks(mime[0]);
            m_mimeToExtensions.add(mime[0], exts);
            if (mime.size() > 2)
                m_mimeToDescriptions.add(mime[0], mime[2]);
        }
    }
*/
}

#endif


}

