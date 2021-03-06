/*##############################################################################

    Copyright (C) 2011 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
############################################################################## */

#include "jfile.hpp" 
#include "hqlerror.hpp"
#include "hqlcollect.hpp"
#include "hqlrepository.hpp"
#include "hqlplugins.hpp"
#include "hqlplugininfo.hpp"

namespace repositoryCommon {

IEclRepository * loadPlugins(const char * pluginPath)
{
    MultiErrorReceiver errs;
    IEclRepository * plugins = createNewSourceFileEclRepository(&errs, pluginPath, ESFallowplugins, (unsigned) -1);//Preload implicits/dlls
    if (errs.errCount())
    {
        StringBuffer s;
        DBGLOG(0,"Errors in plugins: %s", errs.toString(s).str());
    }
    return plugins;
}

IPropertyTree * createPluginPropertyTree(IEclRepository * plugins, bool includeModuleText)
{
    HqlParseContext parseCtx(plugins, NULL);
    HqlLookupContext ctx(parseCtx, NULL);
    HqlScopeArray scopes;
    getRootScopes(scopes, plugins, ctx);

    Owned<IPropertyTree> map = createPTree("Plugins", ipt_caseInsensitive);
    ForEachItemIn(idx, scopes)
    {
        IHqlScope * module = &scopes.item(idx);
        unsigned flags = module->getPropInt(flagsAtom, 0);
        IPropertyTree* prop = createPTree("Module", ipt_caseInsensitive);
        prop->setProp("@name", module->queryFullName());
        prop->setProp("@path", module->querySourcePath()->str());
        prop->setPropInt("@access", module->getPropInt(accessAtom, 3));
        prop->setPropInt("@timestamp", 1);
        prop->setPropInt("@flags", flags);
        if (flags & PLUGIN_DLL_MODULE)
        {
            StringBuffer b;
            module->getProp(versionAtom, b.clear());
            prop->setProp("@version", b.str());

            StringBuffer pluginFullName;
            module->getProp(pluginAtom, pluginFullName);
            prop->setProp("@fullname", pluginFullName.str());   //eclserver needs path and filename

            StringBuffer pluginName(pluginFullName);
            getFileNameOnly(pluginName, false);
            prop->setProp("@plugin", pluginName.str());         //esp only needs filename
        }
        if (includeModuleText)
        {
            IHqlExpression * expr = queryExpression(module);
            IFileContents * definition = expr->queryDefinitionText();
            if (definition)
            {
                StringAttr text;
                text.set(definition->getText(), definition->length());
                prop->setProp("Text", text);
            }
        }
        map->addPropTree("Module", prop);
    }
    return map.getClear();
}

//-------------------------------------------------------------------------------------------------

IPropertyTree * getPlugin(IPropertyTree * p, IEclRepository * plugins, const char* modname, bool load)
{
    if (!p)
        return NULL;

    IPropertyTree* plugin = p->queryPropTree(StringBuffer("./Module[@name=\"").append(modname).append("\"]").str());
    if(!plugin)
        return 0;

    if(load && !plugin->getPropInt("@loaded",0))
    {
        HqlParseContext parseCtx(plugins, NULL);
        HqlLookupContext GHMOREctx(parseCtx, NULL);
        Owned<IHqlScope> resolved = getResolveDottedScope(modname, LSFpublic, GHMOREctx);
        if (resolved)
            exportSymbols(plugin, resolved, GHMOREctx);
        plugin->setPropInt("@loaded",1);
    }
    return LINK(plugin);
}

//-------------------------------------------------------------------------------------------------



}   //namespace repositoryCommon
