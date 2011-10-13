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

#ifndef __HQLINLINE_HPP_
#define __HQLINLINE_HPP_

class ActivityInstance;
class SerializationRow;
class EvalContext;

// A parent extract represents the set of fields etc. which are used from the parent activity,
// which are local to the point that the executeChildActivity() is called.  The type indicates
// the reason it is being created.
class ParentExtract : public HqlExprAssociation
{
public:
    ParentExtract(HqlCppTranslator & _translator, PEtype _type, GraphLocalisation _localisation, EvalContext * _container=NULL);
    ~ParentExtract();

//HqlExprAssociation
    virtual AssocKind getKind() { return AssocExtract; }

    void beginCreateExtract(BuildCtx & buildctx, bool doDeclare);
    void beginNestedExtract(BuildCtx & clonectx);
    void beginReuseExtract();
    void endCreateExtract(CHqlBoundExpr & boundExtract);
    void endUseExtract(BuildCtx & ctx);
    IHqlExpression * queryExtractName()             { return boundExtract.expr; }

    bool canEvaluate(IHqlExpression * expr);
    bool canSerializeFields() const { return buildctx != NULL; }
    void associateCursors(BuildCtx & declarectx, BuildCtx & evalctx, GraphLocalisation childLocalisation);
    void beginChildActivity(BuildCtx & declareCtx, BuildCtx & startCtx, GraphLocalisation childLocalisation, IHqlExpression * colocal, bool nested, bool ignoreSelf, ActivityInstance * activityRequiringCast);
    void endChildActivity();

    void addSerializedExpression(IHqlExpression * value, ITypeInfo * type);
    void buildAssign(IHqlExpression * serializedTarget, IHqlExpression * originalValue);
    AliasKind evaluateExpression(BuildCtx & ctx, IHqlExpression * value, CHqlBoundExpr & tgt, IHqlExpression * colocal, bool evaluateLocally);

    void setAllowDestructor() { canDestroyExtract = true; }
    inline GraphLocalisation queryLocalisation() { return localisation; }

    bool requiresOnStart() const;
    bool insideChildQuery() const;

protected:
    void ensureAccessible(BuildCtx & ctx, IHqlExpression * expr, const CHqlBoundExpr & bound, CHqlBoundExpr & tgt, IHqlExpression * colocal);
    void gatherActiveRows(BuildCtx & ctx);

protected:
    HqlCppTranslator & translator;
    EvalContext * container;
    SerializationRow * serialization;           // fields that are serialized to the children
    SerializationRow * childSerialization;      // same serialization, but as it is bound in the child.

    CursorArray colocalBoundCursors;            // what rows/cursors are available at the point of executeChildGraph()
    CursorArray nonlocalBoundCursors;           // what rows/cursors are available at the point of executeChildGraph()
    GraphLocalisation localisation;             // what kind of localisation do ALL the children have?

    CursorArray inheritedCursors;
    CursorArray localCursors;       // does not include colocal
    CursorArray cursorToBind;
    CHqlBoundExpr boundBuilder;     // may have wrapper, or be a char[n]
    CHqlBoundExpr boundExtract;     // always a reference to a row. for "extract"
    Owned<BuildCtx> buildctx;       // may be null if nested extract
    PEtype type;
    bool canDestroyExtract;
};


class CtxCollection : public CInterface
{
public:
    CtxCollection(BuildCtx & _declareCtx)   : clonectx(_declareCtx), childctx(_declareCtx), declarectx(_declareCtx) {}

    void createFunctionStructure(HqlCppTranslator & translator, BuildCtx & ctx, bool canEvaluate, const char * serializeFunc);

public:
    BuildCtx clonectx;
    BuildCtx childctx;      // child.onCreate() is called from here..
    BuildCtx declarectx;

    //following are always null for nested classes, always created for others.
    Owned<BuildCtx> evalctx;
    Owned<BuildCtx> serializectx;
    Owned<BuildCtx> deserializectx;
};


//A potential location for an extract to be created...
class EvalContext : public HqlExprAssociation
{
public:
    EvalContext(HqlCppTranslator & _translator, ParentExtract * _parentExtract, EvalContext * _parent);

    virtual AssocKind getKind() { return AssocExtractContext; }

    virtual IHqlExpression * createGraphLookup(unique_id_t id, bool isChild);
    virtual AliasKind evaluateExpression(BuildCtx & ctx, IHqlExpression * value, CHqlBoundExpr & tgt, bool evaluateLocally) = 0;
    virtual bool isColocal()                                { return true; }
    virtual ActivityInstance * queryActivity();
    virtual bool isLibraryContext()                         { return false; }

    virtual void tempCompatiablityEnsureSerialized(const CHqlBoundTarget & tgt) = 0;
    virtual bool getInvariantMemberContext(BuildCtx * ctx, BuildCtx * * declarectx, BuildCtx * * initctx, bool isIndependentMaybeShared, bool invariantEachStart) { return false; }

    void ensureContextAvailable()                           { ensureHelpersExist(); }
    virtual bool evaluateInParent(BuildCtx & ctx, IHqlExpression * expr, bool hasOnStart);
    bool hasParent()                                        { return parent != NULL; }
    bool needToEvaluateLocally(BuildCtx & ctx, IHqlExpression * expr);

public://only used by friends
    virtual void callNestedHelpers(const char * memberName) = 0;
    virtual void ensureHelpersExist() = 0;
    virtual bool isRowInvariant(IHqlExpression * expr)      { return false; }

    bool requiresOnStart() const;
    bool insideChildQuery() const;

protected:
    Owned<ParentExtract> parentExtract;         // extract of the parent EvalContext
    HqlCppTranslator & translator;
    EvalContext * parent;
    OwnedHqlExpr colocalMember;
};

class ClassEvalContext : public EvalContext
{
    friend class ActivityInstance;
    friend class GlobalClassBuilder;
public:
    ClassEvalContext(HqlCppTranslator & _translator, ParentExtract * _parentExtract, EvalContext * _parent, BuildCtx & createctx, BuildCtx & startctx);

    virtual AliasKind evaluateExpression(BuildCtx & ctx, IHqlExpression * value, CHqlBoundExpr & tgt, bool evaluateLocally);
    virtual bool isRowInvariant(IHqlExpression * expr);

    virtual void tempCompatiablityEnsureSerialized(const CHqlBoundTarget & tgt);
    virtual bool getInvariantMemberContext(BuildCtx * ctx, BuildCtx * * declarectx, BuildCtx * * initctx, bool isIndependentMaybeShared, bool invariantEachStart);

protected:
    void cloneAliasInClass(CtxCollection & ctxs, const CHqlBoundExpr & bound, CHqlBoundExpr & tgt);
    IHqlExpression * cloneExprInClass(CtxCollection & ctxs, IHqlExpression * expr);
    void createMemberAlias(CtxCollection & ctxs, BuildCtx & ctx, IHqlExpression * value, CHqlBoundExpr & tgt);
    void doCallNestedHelpers(const char * member, const char * acticity);
    void ensureSerialized(CtxCollection & ctxs, const CHqlBoundTarget & tgt);

protected:
    CtxCollection onCreate;
    CtxCollection onStart;
};


class GlobalClassEvalContext : public ClassEvalContext
{
public:
    GlobalClassEvalContext(HqlCppTranslator & _translator, ParentExtract * _parentExtract, EvalContext * _parent, BuildCtx & createctx, BuildCtx & startctx);

    virtual void callNestedHelpers(const char * memberName);
    virtual IHqlExpression * createGraphLookup(unique_id_t id, bool isChild) { throwUnexpected(); }
    virtual void ensureHelpersExist();
    virtual bool isColocal()                                { return false; }
};



class ActivityEvalContext : public ClassEvalContext
{
    friend class ActivityInstance;
public:
    ActivityEvalContext(HqlCppTranslator & _translator, ActivityInstance * _activity, ParentExtract * _parentExtract, EvalContext * _parent, IHqlExpression * _colocal, BuildCtx & createctx, BuildCtx & startctx);

    virtual void callNestedHelpers(const char * memberName);
    virtual IHqlExpression * createGraphLookup(unique_id_t id, bool isChild);
    virtual void ensureHelpersExist();
    virtual bool isColocal()                                { return (colocalMember != NULL); }
    virtual ActivityInstance * queryActivity();

    void createMemberAlias(CtxCollection & ctxs, BuildCtx & ctx, IHqlExpression * value, CHqlBoundExpr & tgt);

protected:
    ActivityInstance * activity;
};


class NestedEvalContext : public ClassEvalContext
{
public:
    NestedEvalContext(HqlCppTranslator & _translator, const char * _memberName, ParentExtract * _parentExtract, EvalContext * _parent, IHqlExpression * _colocal, BuildCtx & createctx, BuildCtx & startctx);

    virtual void callNestedHelpers(const char * memberName);
    virtual IHqlExpression * createGraphLookup(unique_id_t id, bool isChild);
    virtual void ensureHelpersExist();

    void initContext();
    virtual bool evaluateInParent(BuildCtx & ctx, IHqlExpression * expr, bool hasOnStart);

protected:
    bool helpersExist;

protected:
    StringAttr memberName;
};



class MemberEvalContext : public EvalContext
{
public:
    MemberEvalContext(HqlCppTranslator & _translator, ParentExtract * _parentExtract, EvalContext * _parent, BuildCtx & _ctx);

    virtual void callNestedHelpers(const char * memberName);
    virtual void ensureHelpersExist();
    virtual bool isRowInvariant(IHqlExpression * expr);

    virtual IHqlExpression * createGraphLookup(unique_id_t id, bool isChild);
    virtual AliasKind evaluateExpression(BuildCtx & ctx, IHqlExpression * value, CHqlBoundExpr & tgt, bool evaluateLocally);

    virtual bool getInvariantMemberContext(BuildCtx * ctx, BuildCtx * * declarectx, BuildCtx * * initctx, bool isIndependentMaybeShared, bool invariantEachStart);
    virtual void tempCompatiablityEnsureSerialized(const CHqlBoundTarget & tgt);

    void initContext();

protected:
    BuildCtx ctx;
};

class LibraryEvalContext : public EvalContext
{
public:
    LibraryEvalContext(HqlCppTranslator & _translator);

    virtual AliasKind evaluateExpression(BuildCtx & ctx, IHqlExpression * value, CHqlBoundExpr & tgt, bool evaluateLocally);
    virtual bool isColocal()                                { return false; }
    virtual bool isLibraryContext()                         { return true; }

    virtual void tempCompatiablityEnsureSerialized(const CHqlBoundTarget & tgt);

    void associateExpression(BuildCtx & ctx, IHqlExpression * value);
    void ensureContextAvailable()                           { }

public:
    virtual void callNestedHelpers(const char * memberName)     {}
    virtual void ensureHelpersExist()                           {}

protected:
    HqlExprArray values;
    HqlExprArray bound;
};

#endif