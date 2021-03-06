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
#include "jliball.hpp"
#include "hql.hpp"

#include "platform.h"
#include "jlib.hpp"
#include "jhash.hpp"
#include "jmisc.hpp"
#include "jstream.ipp"
#include "jdebug.hpp"

#include "hql.hpp"
#include "hqlattr.hpp"
#include "hqlfold.hpp"
#include "hqlthql.hpp"
#include "hqltrans.ipp"
#include "hqlutil.hpp"
#include "hqlcpp.ipp"
#include "hqlcatom.hpp"
#include "hqlcpputil.hpp"
#include "hqlgraph.ipp"
#include "hqllib.ipp"
#include "hqlwcpp.hpp"
#include "hqlttcpp.ipp"
#include "hqlinline.hpp"

static IHqlExpression * queryLibraryInputSequence(IHqlExpression * expr)
{
    IHqlExpression * arg = expr->queryChild(0);
    if (arg->isRecord())
        arg = expr->queryChild(1);
    return arg;
}

HqlCppLibrary::HqlCppLibrary(HqlCppTranslator & _translator, IHqlExpression * libraryInterface, ClusterType _clusterType) : translator(_translator), inputMapper(libraryInterface), clusterType(_clusterType)
{
    assertex(libraryInterface->getOperator() == no_funcdef);
    scopeExpr = libraryInterface->queryChild(0);
    assertex(scopeExpr->getOperator() == no_virtualscope);

    streamedCount = 0;
    if (clusterType != HThorCluster)
        streamedCount = inputMapper.numStreamedInputs();

    extractOutputs();
}


void HqlCppLibrary::extractOutputs()
{
    HqlExprArray symbols;
    scopeExpr->queryScope()->getSymbols(symbols);

    IHqlScope * scope = scopeExpr->queryScope();
    HqlDummyLookupContext dummyctx(NULL);
    ForEachItemIn(i, symbols)
    {
        IHqlExpression & cur = symbols.item(i);
        if (isExported(&cur))
        {
            _ATOM name = cur.queryName();
            OwnedHqlExpr value = scope->lookupSymbol(name, LSFpublic, dummyctx);

            if (value && !value->isFunction())
            {
                if (value->isDataset() || value->isDatarow() || value->queryType()->isScalar())
                {
                    OwnedHqlExpr null = createNullExpr(value);
                    outputs.append(*cur.cloneAllAnnotations(null));
                }
            }
        }
    }
    outputs.sort(compareSymbolsByName);
}

unsigned HqlCppLibrary::getInterfaceHash() const
{
    unsigned crc = getHash(outputs, 0x12345678);            // only hashes type and name, not implementation
    crc = getHash(inputMapper.queryRealParameters(), crc);
    if (translator.queryOptions().implicitLinkedChildRows)
        crc ^= 0x456271;
    if (crc == 0) crc = 0x87654321;                         // ensure it is non zero.
    return crc;
}

unsigned HqlCppLibrary::getHash(const HqlExprArray & values, unsigned crc) const
{
    unsigned num = values.ordinality();
    crc = hashc((const byte *)&num, sizeof(num), crc);
    ForEachItemIn(i, values)
    {
        IHqlExpression & cur = values.item(i);

        //names are significant because inputs/outputs are ordered by name
        const char * name = cur.queryName()->str();
        crc = hashnc((const byte *)name, strlen(name), crc);

        ITypeInfo * type = cur.queryType();
        byte tc = type->getTypeCode();
        switch (tc)
        {
        case type_record:
        case type_row:
        case type_table:
        case type_groupedtable:
            {
                OwnedHqlExpr normalizedRecord = normalizeRecord(translator, cur.queryRecord());
                OwnedHqlExpr serialized = getSerializedForm(normalizedRecord);
                unsigned recordCrc = getExpressionCRC(serialized);
                crc = hashc((const byte *)&tc, sizeof(tc), crc);
                crc = hashc((const byte *)&recordCrc, sizeof(recordCrc), crc);
                break;
            }
        default:
            {
                size32_t size = type->getSize();
                crc = hashc((const byte *)&tc, sizeof(tc), crc);
                crc = hashc((const byte *)&size, sizeof(size), crc);
                break;
            }
        }
    }
    return crc;
}


unsigned HqlCppLibrary::queryOutputIndex(_ATOM name) const
{
    ForEachItemIn(i, outputs)
    {
        if (outputs.item(i).queryName() == name)
            return i;
    }
    return NotFound;
}


HqlCppLibraryImplementation::HqlCppLibraryImplementation(HqlCppTranslator & _translator, IHqlExpression * libraryInterface, IHqlExpression * _libraryId, ClusterType _clusterType)
: HqlCppLibrary(_translator, libraryInterface, _clusterType), libraryId(_libraryId)
{
    inputMapper.mapRealToLogical(inputExprs, logicalParams, libraryId, (clusterType != HThorCluster), translator.targetThor());
}


HqlCppLibraryInstance::HqlCppLibraryInstance(HqlCppTranslator & translator, IHqlExpression * _instanceExpr, ClusterType clusterType) : instanceExpr(_instanceExpr)
{
    assertex(instanceExpr->getOperator() == no_libraryscopeinstance);
    libraryFuncdef = instanceExpr->queryDefinition();
    assertex(libraryFuncdef->getOperator() == no_funcdef);
    IHqlExpression * libraryScope = libraryFuncdef->queryChild(0);
    assertex(libraryScope->getOperator() == no_libraryscope);
    IHqlExpression * libraryInterface = queryPropertyChild(libraryScope, implementsAtom, 0);
    assertex(libraryInterface);

    library.setown(new HqlCppLibrary(translator, libraryInterface, clusterType));

    assertex(library->totalInputs() == (libraryFuncdef->queryChild(1)->numChildren()));
}



//---------------------------------------------------------------------------

static HqlTransformerInfo hqlLibraryTransformerInfo("HqlLibraryTransformer");
class HqlLibraryTransformer: public QuickHqlTransformer
{
public:
    HqlLibraryTransformer(IConstWorkUnit * _wu, bool _isLibrary) 
        : QuickHqlTransformer(hqlLibraryTransformerInfo, NULL), wu(_wu)
    {
        ignoreFirstScope = _isLibrary;
    }

    IHqlExpression * doTransformLibrarySelect(IHqlExpression * expr)
    {
        //Map the new attribute
        IHqlExpression * oldModule = expr->queryChild(1);
        OwnedHqlExpr newModule = transform(oldModule);
        if (oldModule == newModule)
            return LINK(expr);

        _ATOM attrName = expr->queryChild(3)->queryName();
        HqlDummyLookupContext dummyctx(NULL);
        OwnedHqlExpr value = newModule->queryScope()->lookupSymbol(attrName, makeLookupFlags(true, expr->hasProperty(ignoreBaseAtom), false), dummyctx);
        assertex(value != NULL);
        IHqlExpression * oldAttr = expr->queryChild(2);
        if (oldAttr->isDataset() || oldAttr->isDatarow())
            return value.getClear();

        assertex(value->isDataset());
        IHqlExpression * field = value->queryRecord()->queryChild(0);
        OwnedHqlExpr select = createRow(no_selectnth, value.getClear(), createConstantOne());
        return createSelectExpr(select.getClear(), LINK(field));        // no newAtom because not normalised yet
    }

    virtual IHqlExpression * createTransformedBody(IHqlExpression * expr)
    {
        switch (expr->getOperator())
        {
        case no_libraryselect:
            return doTransformLibrarySelect(expr);
        }

        return QuickHqlTransformer::createTransformedBody(expr);
    }

protected:
    IHqlExpression * createSimplifiedLibrary(IHqlExpression * libraryExpr)
    {
        IHqlScope * oldScope = libraryExpr->queryScope();
        IHqlScope * lookupScope = oldScope;
        IHqlExpression * interfaceExpr = queryPropertyChild(libraryExpr, implementsAtom, 0);
        if (interfaceExpr)
            lookupScope = interfaceExpr->queryScope();

        HqlExprArray symbols, newSymbols;
        lookupScope->getSymbols(symbols);

        HqlDummyLookupContext dummyctx(NULL);
        ForEachItemIn(i, symbols)
        {
            IHqlExpression & cur = symbols.item(i);
            if (isExported(&cur))
            {
                _ATOM name = cur.queryName();
                OwnedHqlExpr oldSymbol = oldScope->lookupSymbol(name, LSFpublic, dummyctx);
                OwnedHqlExpr newValue;
                ITypeInfo * type = oldSymbol->queryType();
                if (oldSymbol->isDataset() || oldSymbol->isDatarow())
                {
                    newValue.setown(createNullExpr(oldSymbol));
                }
                else
                {
                    //Convert a scalar to a select from a dataset
                    OwnedHqlExpr field = createField(unknownAtom, LINK(type), NULL, NULL);
                    OwnedHqlExpr newRecord = createRecord(field);
                    OwnedHqlExpr ds = createDataset(no_null, LINK(newRecord));
                    newValue.setown(createNullExpr(ds));
                }

                if (oldSymbol->isFunction())
                {
                    // Should have been caught in the parser..  Following code should be correct if we ever work out how to implement
                    throwUnexpected();
                    HqlExprArray parms;
                    unwindChildren(parms, oldSymbol, 1);
                    IHqlExpression * formals = createSortList(parms);
                    newValue.setown(createFunctionDefinition(name, newValue.getClear(), formals, NULL, NULL));
                }

                IHqlExpression * newSym = oldSymbol->cloneAllAnnotations(newValue);
                newSymbols.append(*newSym);
            }
        }

        HqlExprArray children;
        unwindChildren(children, libraryExpr);
        return queryExpression(oldScope->clone(children, newSymbols));
    }

protected:
    IConstWorkUnit * wu;
    bool ignoreFirstScope;
};


class HqlEmbeddedLibraryTransformer: public HqlLibraryTransformer
{
public:
    HqlEmbeddedLibraryTransformer(IConstWorkUnit * _wu, HqlExprArray & _internalLibraries, bool _isLibrary) 
        : HqlLibraryTransformer(_wu, _isLibrary), internalLibraries(_internalLibraries)
    {
    }

    virtual IHqlExpression * createTransformedBody(IHqlExpression * expr)
    {
        switch (expr->getOperator())
        {
        case no_libraryscopeinstance:
            {
                IHqlExpression * scopeFunc = expr->queryDefinition();
                assertex(scopeFunc->getOperator() == no_funcdef);
                IHqlExpression * moduleExpr = scopeFunc->queryChild(0);
                assertex(moduleExpr->getOperator() == no_libraryscope);
                IHqlExpression * internalExpr = moduleExpr->queryProperty(internalAtom);
                if (internalExpr)
                {
                    IHqlExpression * nameExpr = moduleExpr->queryProperty(nameAtom);
                    if (!matchedInternalLibraries.contains(*nameExpr))
                    {
                        internalLibraries.append(*transformEmbeddedLibrary(scopeFunc));
                        matchedInternalLibraries.append(*LINK(nameExpr));
                    }
                    //remove the parameter from the internal attribute from the module that is being called.
                    //so save in subsequent translation
                    OwnedHqlExpr newModuleExpr = replaceOwnedProperty(moduleExpr, createAttribute(internalAtom));
                    OwnedHqlExpr newScopeFunc = replaceChild(scopeFunc, 0, newModuleExpr);
                    HqlExprArray instanceArgs;
                    unwindChildren(instanceArgs, expr);
                    return createLibraryInstance(LINK(newScopeFunc), instanceArgs);
                }
                else
                    return LINK(expr);
                //otherwise already transformed....
                break;
            }
        case no_libraryscope:
            if (!ignoreFirstScope)
            {
                //Now create a simplified no_library scope with null values for each of the values of the exported symbols
                //Don't walk contents because that is an implementation detail, which could change
                return createSimplifiedLibrary(expr);
            }
            ignoreFirstScope = false;
            break;
        }

        return HqlLibraryTransformer::createTransformedBody(expr);
    }

    IHqlExpression * transformEmbeddedLibrary(IHqlExpression * expr)
    {
        //avoid special casing above
        assertex(expr->getOperator() == no_funcdef);
        IHqlExpression * library = expr->queryChild(0);
        HqlExprArray args;
        args.append(*QuickHqlTransformer::createTransformedBody(library));
        unwindChildren(args, expr, 1);
        return expr->clone(args);
    }

protected:
    HqlExprArray & internalLibraries;
    HqlExprArray matchedInternalLibraries;
};

//---------------------------------------------------------------------------


void HqlCppTranslator::processEmbeddedLibraries(HqlExprArray & exprs, HqlExprArray & internalLibraries, bool isLibrary)
{
    HqlExprArray transformed;

    HqlEmbeddedLibraryTransformer transformer(wu(), internalLibraries, isLibrary);
    ForEachItemIn(i, exprs)
        transformed.append(*transformer.transform(&exprs.item(i)));

    replaceArray(exprs, transformed);
}

//---------------------------------------------------------------------------

static IHqlExpression * splitSelect(IHqlExpression * & rootDataset, IHqlExpression * expr, IHqlExpression * selSeq)
{
    assertex(expr->getOperator() == no_select);
    IHqlExpression * ds = expr->queryChild(0);
    IHqlExpression * field = expr->queryChild(1);

    IHqlExpression * attrSelector;
    if (ds->isDatarow() && (ds->getOperator() == no_select))
    {
        attrSelector = splitSelect(rootDataset, ds, selSeq);
    }
    else
    {
        rootDataset = ds;
        attrSelector = createSelector(no_left, ds, selSeq);
    }
    return createSelectExpr(attrSelector, LINK(field));
}

static IHqlExpression * convertScalarToDataset(IHqlExpression * expr)
{
    switch (expr->getOperator())
    {
    case NO_AGGREGATE:
        return convertScalarAggregateToDataset(expr);
    case no_select:
        {
            //Project dataset down to a single field...
            IHqlExpression * ds;
            OwnedHqlExpr selSeq = createSelectorSequence();
            OwnedHqlExpr newExpr = splitSelect(ds, expr, selSeq);
            IHqlExpression * field = expr->queryChild(1);
            OwnedHqlExpr record = createRecord(field);
            OwnedHqlExpr assign = createAssign(createSelectExpr(createSelector(no_self, record, NULL), LINK(field)), LINK(newExpr));
            OwnedHqlExpr row = createRow(no_projectrow, LINK(ds), createComma(createValue(no_transform, makeTransformType(record->getType()), LINK(assign)), LINK(selSeq)));
            return LINK(row);
            //Following is more strictly correct, but messes up the resourcing.
            //return createDatasetFromRow(LINK(row));
        }
        break;
    }

    OwnedHqlExpr field = createField(valueAtom, expr->getType(), NULL, NULL);
    OwnedHqlExpr record = createRecord(field);
    OwnedHqlExpr assign = createAssign(createSelectExpr(createSelector(no_self, record, NULL), LINK(field)), LINK(expr));
    OwnedHqlExpr row = createRow(no_createrow, createValue(no_transform, makeTransformType(record->getType()), LINK(assign)));
    return createDatasetFromRow(LINK(row));
}

void HqlCppLibraryImplementation::mapLogicalToImplementation(HqlExprArray & exprs, IHqlExpression * libraryExpr)
{
    //First replace parameters with streamed inputs, and no_libraryinputs, by creating some psuedo-arguments,
    //and then binding them.
    HqlExprArray actuals;
    appendArray(actuals, logicalParams);
    OwnedHqlExpr bound = createBoundFunction(NULL, libraryExpr, actuals, NULL, true);
    IHqlScope * scope = bound->queryScope();
    assertex(scope);

    //Now resolve each of the outputs in the transformed module
    HqlDummyLookupContext dummyctx(NULL);
    unsigned numInputs = numStreamedInputs();
    ForEachItemIn(i, outputs)
    {
        IHqlExpression & curOutput = outputs.item(i);
        OwnedHqlExpr output = scope->lookupSymbol(curOutput.queryName(), LSFpublic, dummyctx);

        // Do a global replace of input(n) with no_getgraphresult(n), and no_param with no_
        if (!output->isDatarow() && !output->isDataset())
            output.setown(convertScalarToDataset(output));
        
        HqlExprArray args;
        args.append(*LINK(output));
        args.append(*LINK(libraryId));
        args.append(*getSizetConstant(i+numInputs));
        exprs.append(*createValue(no_setgraphresult, makeVoidType(), args));
    }

}

//---------------------------------------------------------------------------

void ThorBoundLibraryActivity::noteOutputUsed(_ATOM name)
{
    unsigned matchIndex = libraryInstance->library->queryOutputIndex(name);
    assertex(matchIndex != NotFound);
    addGraphAttributeInt(graphNode, "_outputUsed", matchIndex);
}

ABoundActivity * HqlCppTranslator::doBuildActivityLibrarySelect(BuildCtx & ctx, IHqlExpression * expr)
{
    IHqlExpression * module = expr->queryChild(1);

    ThorBoundLibraryActivity * callInstance = static_cast<ThorBoundLibraryActivity *>(buildCachedActivity(ctx, module));

    callInstance->noteOutputUsed(expr->queryChild(3)->queryName());

    return callInstance;
}


//---------------------------------------------------------------------------

void HqlCppTranslator::buildLibraryInstanceExtract(BuildCtx & ctx, HqlCppLibraryInstance * libraryInstance)
{
    BuildCtx subctx(ctx);
    subctx.addQuotedCompound("virtual void createParentExtract(rtlRowBuilder & builder)");


    BuildCtx beforeBuilderCtx(subctx);
    beforeBuilderCtx.addGroup();
    Owned<ParentExtract> extractBuilder = createExtractBuilder(subctx, PETlibrary, NULL, GraphNonLocal, false);

    StringBuffer s;
    s.append("rtlRowBuilder & ");
    generateExprCpp(s, extractBuilder->queryExtractName());
    s.append(" = builder;");
    beforeBuilderCtx.addQuoted(s);

    beginExtract(subctx, extractBuilder);

    //Ensure all the values are added to the serialization in the correct order 
    CHqlBoundExpr dummyTarget;
    unsigned numParams = libraryInstance->numParameters();
    for (unsigned i2 = libraryInstance->numStreamedInputs(); i2 < numParams; i2++)
    {
        IHqlExpression * parameter = libraryInstance->queryParameter(i2);
        extractBuilder->addSerializedExpression(libraryInstance->queryActual(i2), parameter->queryType());
    }

    endExtract(subctx, extractBuilder);
}

ABoundActivity * HqlCppTranslator::doBuildActivityLibraryInstance(BuildCtx & ctx, IHqlExpression * expr)
{
    Owned<HqlCppLibraryInstance> libraryInstance = new HqlCppLibraryInstance(*this, expr, targetClusterType);

    CIArray boundInputs;

    unsigned numStreamed = libraryInstance->numStreamedInputs();
    for (unsigned i1=0; i1 < numStreamed; i1++)
        boundInputs.append(*buildCachedActivity(ctx, libraryInstance->queryActual(i1)));

    IHqlExpression * moduleFunction = expr->queryDefinition();      // no_funcdef
    IHqlExpression * module = moduleFunction->queryChild(0);
    assertex(module->getOperator() == no_libraryscope);
    IHqlExpression * nameAttr = module->queryProperty(nameAtom);
    OwnedHqlExpr name = foldHqlExpression(nameAttr->queryChild(0));
    IValue * nameValue = name->queryValue();
    IHqlExpression * originalName = queryPropertyChild(module, _original_Atom, 0);

    StringBuffer libraryName;
    if (nameValue)
        nameValue->getStringValue(libraryName);

    Owned<ActivityInstance> instance = new ActivityInstance(*this, ctx, TAKlibrarycall, expr, "LibraryCall");
    StringBuffer graphLabel;
    if (originalName)
    {
        StringBuffer temp;
        temp.append(originalName->queryName()).toLowerCase();
        graphLabel.append("Library").newline().append(temp);
    }
    else if (nameValue)
        graphLabel.append("Library").newline().append(libraryName);

    if (graphLabel.length())
        instance->graphLabel.set(graphLabel.str());

    buildActivityFramework(instance);

    buildInstancePrefix(instance);

    buildLibraryInstanceExtract(instance->startctx, libraryInstance);

    //MORE: Need to call functions to add the extract
    const HqlCppLibrary * library = libraryInstance->library;

    if (nameValue)
    {
        instance->addAttribute("libname", libraryName.str());
        Owned<IWULibrary> wulib = wu()->updateLibraryByName(libraryName.str());
        wulib->addActivity((unsigned)instance->activityId);
    }

    instance->addAttributeInt("_interfaceHash", library->getInterfaceHash());
    instance->addAttributeBool("embedded", module->hasProperty(internalAtom));
    instance->addAttributeInt("_maxOutputs", library->outputs.ordinality());
    if (!targetHThor())
        instance->addAttributeInt("_graphid", nextActivityId());            // reserve an id...

    // A debugging option to make it clearer how a library is being called.
    if (options.addLibraryInputsToGraph)
    {
        unsigned numParams = libraryInstance->numParameters();
        for (unsigned iIn = 0; iIn < numParams; iIn++)
        {
            IHqlExpression * parameter = libraryInstance->queryParameter(iIn);
            StringBuffer paramName;
            StringBuffer paramEcl;
            paramName.append("input").append(iIn).append("__").append(parameter->queryName());
            toECL(libraryInstance->queryActual(iIn), paramEcl, false, true);
            instance->addAttribute(paramName, paramEcl);
        }
    }

    StringBuffer s;
    BuildCtx metactx(instance->classctx);
    metactx.addQuotedCompound("virtual IOutputMetaData * queryOutputMeta(unsigned whichOutput)");
    BuildCtx switchctx(metactx);
    switchctx.addQuotedCompound("switch (whichOutput)");

    HqlDummyLookupContext dummyCtx(NULL);
    IHqlScope * moduleScope = module->queryScope();
    ForEachItemIn(iout, library->outputs)
    {
        IHqlExpression & cur = library->outputs.item(iout);
        OwnedHqlExpr dataset = moduleScope->lookupSymbol(cur.queryName(), LSFpublic, dummyCtx);
        assertex(dataset && dataset->queryRecord());
        MetaInstance meta(*this, dataset->queryRecord(), isGrouped(dataset));
        buildMetaInfo(meta);
        switchctx.addQuoted(s.clear().append("case ").append(iout).append(": return &").append(meta.queryInstanceObject()).append(";"));
    }
    metactx.addReturn(queryQuotedNullExpr());

    //Library Name must be onCreate invariant
    BuildCtx namectx(instance->createctx);
    namectx.addQuotedCompound("virtual char * getLibraryName()");
    buildReturn(namectx, name, unknownVarStringType);

    buildInstanceSuffix(instance);

    ForEachItemIn(idx2, boundInputs)
        buildConnectInputOutput(ctx, instance, (ABoundActivity *)&boundInputs.item(idx2), 0, idx2);

    Owned<ABoundActivity> boundInstance = instance->getBoundActivity();
    return new ThorBoundLibraryActivity(boundInstance, instance->graphNode, libraryInstance);
}



void HqlCppTranslator::buildLibraryGraph(BuildCtx & ctx, IHqlExpression * expr, const char * graphName)
{
    OwnedHqlExpr resourced = getResourcedGraph(expr->queryChild(0), NULL);

    beginGraph(ctx, graphName);

    traceExpression("beforeGenerate", resourced);
    BuildCtx initctx(ctx);
    initctx.addGroup();
    initctx.addFilter(queryBoolExpr(false));

    Owned<LibraryEvalContext> libraryContext = new LibraryEvalContext(*this);
    initctx.associate(*libraryContext);

    unsigned numParams = outputLibrary->totalInputs();
    for (unsigned i1 = outputLibrary->numStreamedInputs(); i1 < numParams; i1++)
    {
        IHqlExpression & parameter = *outputLibrary->queryInputExpr(i1);
        libraryContext->associateExpression(initctx, &parameter);
    }

    Owned<ParentExtract> extractBuilder = createExtractBuilder(initctx, PETlibrary, outputLibraryId, GraphRemote, false);
    beginExtract(initctx, extractBuilder);

    BuildCtx evalctx(ctx);
    evalctx.addGroup();
    evalctx.addFilter(queryBoolExpr(false));

    //Ensure all the values are added to the serialization in the correct order by creating a dummy context and then
    //evaluating each parameter in term.  Slightly ugly - it would be better using different calls.
    extractBuilder->associateCursors(evalctx, evalctx, GraphNonLocal);

    CHqlBoundExpr dummyTarget;
    for (unsigned i2 = outputLibrary->numStreamedInputs(); i2 < numParams; i2++)
    {
        IHqlExpression * parameter = outputLibrary->queryInputExpr(i2);
        extractBuilder->evaluateExpression(evalctx, parameter, dummyTarget, NULL, false);
    }

    BuildCtx graphctx(initctx);
    activeGraphCtx = &ctx;
    doBuildThorSubGraph(graphctx, resourced, SubGraphRoot, (unsigned)getIntValue(outputLibraryId->queryChild(0)), outputLibraryId);
    activeGraphCtx = NULL;

    endExtract(initctx, extractBuilder);
    endGraph();
}


//---------------------------------------------------------------------------


LibraryEvalContext::LibraryEvalContext(HqlCppTranslator & _translator) : EvalContext(_translator, NULL, NULL)
{
}

AliasKind LibraryEvalContext::evaluateExpression(BuildCtx & ctx, IHqlExpression * value, CHqlBoundExpr & tgt, bool evaluateLocally)
{
    if (value->getOperator() == no_libraryinput)
    {
        IHqlExpression * seq = queryLibraryInputSequence(value);
        unsigned match = values.find(*seq);
        assertex(match != NotFound);
        tgt.setFromTranslated(&bound.item(match));
    }
    else
        translator.buildTempExpr(ctx, value, tgt);
    return RuntimeAlias;
}

void LibraryEvalContext::associateExpression(BuildCtx & ctx, IHqlExpression * value)
{
    //Add the association by sequence number, because this is pre record normalization.
    assertex(value->getOperator() == no_libraryinput);
    IHqlExpression * seq = queryLibraryInputSequence(value);
    assertex(!values.contains(*seq));

    CHqlBoundTarget tempTarget;
    translator.createTempFor(ctx, value, tempTarget);
    values.append(*LINK(seq));

    CHqlBoundExpr tgt;
    tgt.setFromTarget(tempTarget);
    bound.append(*tgt.getTranslatedExpr());
}

void LibraryEvalContext::tempCompatiablityEnsureSerialized(const CHqlBoundTarget & tgt)
{
    throwUnexpected();
}

