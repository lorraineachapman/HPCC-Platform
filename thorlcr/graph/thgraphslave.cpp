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

#include "jlib.hpp"
#include "jlzw.hpp"
#include "jhtree.hpp"
#include "daclient.hpp"
#include "commonext.hpp"
#include "thorplugin.hpp"
#include "thcodectx.hpp"
#include "thorport.hpp"
#include "slwatchdog.hpp"
#include "thgraphslave.hpp"

//////////////////////////////////

class CBarrierSlave : public CInterface, implements IBarrier
{
    mptag_t tag;
    Linked<ICommunicator> comm;
    bool receiving;

public:
    IMPLEMENT_IINTERFACE;

    CBarrierSlave(ICommunicator &_comm, mptag_t _tag) : comm(&_comm), tag(_tag)
    {
        receiving = false;
    }
    virtual bool wait(bool exception, unsigned timeout)
    {
        CTimeMon tm(timeout);
        unsigned remaining = timeout;
        CMessageBuffer msg;
        msg.append(false);
        if (INFINITE != timeout && tm.timedout(&remaining))
        {
            if (exception)
                throw createBarrierAbortException();
            else
                return false;
        }
        if (!comm->send(msg, 0, tag, INFINITE != timeout ? remaining : LONGTIMEOUT))
            throw MakeStringException(0, "CBarrierSlave::wait - Timeout sending to master");
        msg.clear();
        if (INFINITE != timeout && tm.timedout(&remaining))
        {
            if (exception)
                throw createBarrierAbortException();
            else
                return false;
        }
        {
            BooleanOnOff onOff(receiving);
            if (!comm->recv(msg, 0, tag, NULL, remaining))
                return false;
        }
        bool aborted;
        msg.read(aborted);
        if (aborted)
        {
            if (exception)
                throw createBarrierAbortException();
            else
                return false;
        }   
        return true;
    }
    virtual void cancel()
    {
        if (receiving)
            comm->cancel(comm->queryGroup().rank(), tag);
        CMessageBuffer msg;
        msg.append(true);
        if (!comm->send(msg, 0, tag, LONGTIMEOUT))
            throw MakeStringException(0, "CBarrierSlave::cancel - Timeout sending to master");
    }
    virtual const mptag_t queryTag() const { return tag; }
};

// 

CSlaveActivity::CSlaveActivity(CGraphElementBase *_container) : CActivityBase(_container)
{
    data = NULL;
    totalCycles = 0;
}

CSlaveActivity::~CSlaveActivity()
{
    inputs.kill();
    outputs.kill();
    if (data) delete [] data;
    ActPrintLog("DESTROYED");
}

void CSlaveActivity::setInput(unsigned index, CActivityBase *inputActivity, unsigned inputOutIdx)
{
    CActivityBase::setInput(index, inputActivity, inputOutIdx);
    Owned<CActivityBase> nullAct;
    if (!inputActivity)
    {
        nullAct.setown(container.factory(TAKnull));
        inputActivity = nullAct; // NB: output of nullAct linked, input of this act will own
    }

    IThorDataLink *outLink = ((CSlaveActivity *)inputActivity)->queryOutput(inputOutIdx);
    assertex(outLink);
    while (inputs.ordinality()<=index) inputs.append(NULL);
    inputs.replace(LINK(outLink), index);
}

IThorDataLink *CSlaveActivity::queryOutput(unsigned index)
{
    if (index>=outputs.ordinality()) return NULL;
    return outputs.item(index);
}

IThorDataLink *CSlaveActivity::queryInput(unsigned index)
{
    if (index>=inputs.ordinality()) return NULL;
    return inputs.item(index);
}

void CSlaveActivity::startInput(IThorDataLink *itdl, const char *extra)
{
    StringBuffer s("Starting input");
    if (extra)
        s.append(" ").append(extra);
    ActPrintLog("%s", s.str());

#ifdef TRACE_STARTSTOP_EXCEPTIONS
    try
    {
        itdl->start();
    }
    catch(IException *e)
    {
        ActPrintLog(e, "%s", s.str());
        throw;
    }
#else
    itdl->start();
#endif
}

void CSlaveActivity::stopInput(IThorDataLink *itdl, const char *extra)
{
    StringBuffer s("Stopping input for");
    if (extra)
        s.append(" ").append(extra);
    ActPrintLog("%s", s.str());

#ifdef TRACE_STARTSTOP_EXCEPTIONS
    try
    {
        itdl->stop();
    }
    catch(IException * e)
    {
        ActPrintLog(e, "%s", s.str());
        throw;
    }
#else
    itdl->stop();
#endif
}

void CSlaveActivity::abort()
{
    CActivityBase::abort();
    CriticalBlock b(crit);
    ForEachItemIn(o, outputs)
    {
        StringBuffer msg("-------->  ");
        msg.append("GraphId = ").append(container.queryOwner().queryGraphId());
        msg.append(" ActivityId = ").append(container.queryId());
        msg.append("  OutputId = ").append(o);
        MemoryBuffer mb;
        outputs.item(o)->dataLinkSerialize(mb); // JCSMORE should add direct method
        rowcount_t count;
        mb.read(count);
        msg.append(": Count = ").append(count);
    }
}

void CSlaveActivity::releaseIOs()
{
//  inputs.kill(); // don't want inputs to die before this dies (release in deconstructor) // JCSMORE not sure why care particularly.
    outputs.kill(); // outputs tend to be self-references, this clears them explicitly, otherwise end up leaking with circular references.
}

void CSlaveActivity::clearConnections()
{
    inputs.kill();
}

MemoryBuffer &CSlaveActivity::queryInitializationData(unsigned slave) const
{
    CriticalBlock b(crit);
    if (!data)
        data = new MemoryBuffer[container.queryJob().querySlaves()];
    CMessageBuffer msg;
    graph_id gid = queryContainer().queryOwner().queryGraphId();
    msg.append(smt_dataReq);
    msg.append(slave);
    msg.append(gid);
    msg.append(container.queryId());
    if (!container.queryJob().queryJobComm().sendRecv(msg, 0, queryContainer().queryJob().querySlaveMpTag(), LONGTIMEOUT))
        throwUnexpected();
    data[slave].swapWith(msg);
    return data[slave];
}

MemoryBuffer &CSlaveActivity::getInitializationData(unsigned slave, MemoryBuffer &mb) const
{
    return mb.append(queryInitializationData(slave));
}

unsigned __int64 CSlaveActivity::queryLocalCycles() const
{
    unsigned __int64 inputCycles = 0;
    if (1 == inputs.ordinality())
    {
        IThorDataLink *input = inputs.item(0);
        CSlaveActivity &inputAct = * (CSlaveActivity *)input->queryFromActivity();
        inputCycles += inputAct.queryTotalCycles();
    }
    else
    {
        if (((unsigned)-1) != container.whichBranch && inputs.isItem(container.whichBranch))
        {
            IThorDataLink *input = inputs.item(container.whichBranch);
            if (input != NULL)
                inputCycles += inputs.item(container.whichBranch)->queryTotalCycles();
        }
        else
        {
            ForEachItemIn(i, inputs)
            {
                IThorDataLink *input = inputs.item(i);
                inputCycles += input->queryTotalCycles();
            }
        }
    }
    if (totalCycles < inputCycles) // not sure how/if possible, but guard against
        return 0;
    return totalCycles-inputCycles;
}

unsigned __int64 CSlaveActivity::queryTotalCycles() const
{
    return totalCycles;
}

void CSlaveActivity::serializeStats(MemoryBuffer &mb)
{
    CriticalBlock b(crit);
    mb.append((unsigned __int64)cycle_to_nanosec(queryLocalCycles()));
    ForEachItemIn(i, outputs)
        outputs.item(i)->dataLinkSerialize(mb);
}

///

// CSlaveGraph

CSlaveGraph::CSlaveGraph(CJobSlave &job) : CGraphBase(job), jobS(job)
{
}

void CSlaveGraph::init(MemoryBuffer &mb)
{
    mb.read(reinit);
    mpTag = job.deserializeMPTag(mb);
    startBarrierTag = job.deserializeMPTag(mb);
    waitBarrierTag = job.deserializeMPTag(mb);
    doneBarrierTag = job.deserializeMPTag(mb);
    startBarrier = job.createBarrier(startBarrierTag);
    waitBarrier = job.createBarrier(waitBarrierTag);
    if (doneBarrierTag != TAG_NULL)
        doneBarrier = job.createBarrier(doneBarrierTag);
    unsigned subCount;
    mb.read(subCount);
    while (subCount--)
    {
        graph_id gid;
        mb.read(gid);
        Owned<CSlaveGraph> subGraph = (CSlaveGraph *)job.getGraph(gid);
        subGraph->init(mb);
    }
}

void CSlaveGraph::initWithActData(MemoryBuffer &in, MemoryBuffer &out)
{
    activity_id id;
    loop
    {
        in.read(id);
        if (0 == id) break;
        CSlaveGraphElement *element = (CSlaveGraphElement *)queryElement(id);
        assertex(element);
        out.append(id);
        out.append((size32_t)0);
        unsigned l = out.length();
        size32_t sz;
        in.read(sz);
        unsigned aread = in.getPos();
        CSlaveActivity *activity = (CSlaveActivity *)element->queryActivity();
        if (activity)
        {
            element->sentActInitData->set(0);
            activity->init(in, out);
        }
        aread = in.getPos()-aread;
        if (aread<sz)
        {
            Owned<IException> e = MakeActivityException(element, TE_SeriailzationError, "Serialization error - activity did not read all serialized data (%d byte(s) remaining)", sz-aread);
            in.readDirect(sz-aread);
            throw e.getClear();
        }
        else if (aread>sz)
            throw MakeActivityException(element, TE_SeriailzationError, "Serialization error - activity read beyond serialized data (%d byte(s))", aread-sz);
        size32_t dl = out.length() - l;
        if (dl)
            out.writeDirect(l-sizeof(size32_t), sizeof(size32_t), &dl);
        else
            out.setLength(l-(sizeof(activity_id)+sizeof(size32_t)));
    }
    out.append((activity_id)0);
}

void CSlaveGraph::recvStartCtx()
{
    if (!sentStartCtx)
    {
        sentStartCtx = true;
        CMessageBuffer msg;
        if (!job.queryJobComm().recv(msg, 0, mpTag, NULL, LONGTIMEOUT))
            throw MakeStringException(0, "Error receiving startCtx data for graph: %"GIDPF"d", graphId);
        activity_id id;
        loop
        {
            msg.read(id);
            if (0 == id) break;
            CSlaveGraphElement *element = (CSlaveGraphElement *)queryElement(id);
            assertex(element);
            element->deserializeStartContext(msg);
        }
    }
}

bool CSlaveGraph::recvActivityInitData()
{
    bool ret = true;
    unsigned needActInit = 0;
    Owned<IThorActivityIterator> iter = getTraverseIterator();
    ForEach(*iter)
    {
        CSlaveGraphElement &element = (CSlaveGraphElement &)iter->query();
        if (!element.sentActInitData->test(0))
            ++needActInit;
    }
    if (needActInit)
    {
        mptag_t replyTag = TAG_NULL;
        unsigned len;
        CMessageBuffer actInitRtnData;
        actInitRtnData.append(false);
        CMessageBuffer msg;
        if (!queryOwner() || isGlobal())
        {
            if (!job.queryJobComm().recv(msg, 0, mpTag, NULL, LONGTIMEOUT))
                throw MakeStringException(0, "Error receiving actinit data for graph: %"GIDPF"d", graphId);
            replyTag = msg.getReplyTag();
            msg.read(len);
        }
        else
        {
            // initialize any for which no data was sent
            msg.append(smt_initActDataReq); // may cause graph to be created at master
            msg.append(queryGraphId());
            Owned<IThorActivityIterator> iter = getTraverseIterator();
            ForEach(*iter)
            {
                CSlaveGraphElement &element = (CSlaveGraphElement &)iter->query();
                if (!element.sentActInitData->test(0))
                    msg.append(element.queryId());
            }
            msg.append((activity_id)0);
            if (!queryJob().queryJobComm().sendRecv(msg, 0, queryJob().querySlaveMpTag(), LONGTIMEOUT))
                throwUnexpected();
            replyTag = job.deserializeMPTag(msg);
            msg.read(len);
        }
        try
        {
            if (len)
            {
                MemoryBuffer actInitData;
                actInitData.append(len, msg.readDirect(len));
                initWithActData(actInitData, actInitRtnData);
            }
            if (queryOwner() && !isGlobal())
            {
                // initialize any for which no data was sent
                Owned<IThorActivityIterator> iter = getTraverseIterator();
                ForEach(*iter)
                {
                    CSlaveGraphElement &element = (CSlaveGraphElement &)iter->query();
                    if (!element.sentActInitData->test(0))
                    {
                        element.sentActInitData->set(0);
                        CSlaveActivity *activity = (CSlaveActivity *)element.queryActivity();
                        if (activity)
                        {
                            MemoryBuffer in, out;
                            activity->init(in, out);
                            assertex(0 == out.length());
                        }
                    }
                }
            }
        }
        catch (IException *e)
        {
            actInitRtnData.clear();
            actInitRtnData.append(true);
            serializeThorException(e, actInitRtnData);
            e->Release();
            ret = false;
        }
        if (!job.queryJobComm().send(actInitRtnData, 0, replyTag, LONGTIMEOUT))
            throw MakeStringException(0, "Timeout sending init data back to master");
    }
    return ret;
}

bool CSlaveGraph::preStart(size32_t parentExtractSz, const byte *parentExtract)
{
    started = true;
    recvStartCtx();
    CGraphBase::preStart(parentExtractSz, parentExtract);

    if (!recvActivityInitData())
        return false;
    connect(); // only now do slave acts. have all their outputs prepared.
    if (isGlobal())
    {
        if (!startBarrier->wait(false))
            return false;
    }
    return true;
}

void CSlaveGraph::start()
{
    bool forceAsync = !queryOwner() || isGlobal();
    Owned<IThorActivityIterator> iter = getSinkIterator();
    unsigned sinks = 0;
    ForEach(*iter)
        ++sinks;
    ForEach(*iter)
    {
        CGraphElementBase &container = iter->query();
        CActivityBase *sinkAct = (CActivityBase *)container.queryActivity();
        --sinks;
        sinkAct->startProcess(forceAsync || 0 != sinks); // async, unless last
    }
    if (!queryOwner())
    {
        if (globals->getPropBool("@watchdogProgressEnabled"))
            jobS.queryProgressHandler()->startGraph(*this);
    }
}

void CSlaveGraph::connect()
{
    CriticalBlock b(progressCrit);
    Owned<IThorActivityIterator> iter = getTraverseIterator();
    ForEach(*iter)
        iter->query().doconnect();
}

void CSlaveGraph::executeSubGraph(size32_t parentExtractSz, const byte *parentExtract)
{
    if (isComplete())
        return;
    Owned<IException> exception;
    try
    {
        CGraphBase::executeSubGraph(parentExtractSz, parentExtract);
    }
    catch (IException *e)
    {
        GraphPrintLog(e, NULL);
        exception.setown(e);
    }
    if (TAG_NULL != executeReplyTag)
    {
        CMessageBuffer msg;
        if (exception.get())
        {
            msg.append(true);
            serializeThorException(exception, msg);
        }
        else
            msg.append(false);
        queryJob().queryJobComm().send(msg, 0, executeReplyTag, LONGTIMEOUT);
    }
    else if (exception)
        throw exception.getClear();
}

bool CSlaveGraph::prepare(size32_t parentExtractSz, const byte *parentExtract, bool checkDependencies, bool shortCircuit, bool async)
{
    CriticalBlock b(progressCrit);
    return CGraphBase::prepare(parentExtractSz, parentExtract, checkDependencies, shortCircuit, async);
}

void CSlaveGraph::create(size32_t parentExtractSz, const byte *parentExtract)
{
    CriticalBlock b(progressCrit);
    if (queryOwner())
    {
        if (isGlobal())
        {
            CMessageBuffer msg;
            // nothing changed if rerunning, unless conditional branches different
            if (!job.queryJobComm().recv(msg, 0, mpTag, NULL, LONGTIMEOUT))
                throw MakeStringException(0, "Error receiving createctx data for graph: %"GIDPF"d", graphId);
            try
            {
                unsigned len;
                msg.read(len);
                if (len)
                {
                    MemoryBuffer initData;
                    initData.append(len, msg.readDirect(len));
                    deserializeCreateContexts(initData);
                }
                msg.clear();
                msg.append(false);
            }
            catch (IException *e)
            {
                msg.clear();
                msg.append(true);
                serializeThorException(e, msg);
            }
            if (!job.queryJobComm().send(msg, 0, msg.getReplyTag(), LONGTIMEOUT))
                throw MakeStringException(0, "Timeout sending init data back to master");
        }
        else
        {
            ForEachItemIn(i, ifs)
            {
                CGraphElementBase &ifElem = ifs.item(i);
                if (ifElem.newWhichBranch)
                {
                    ifElem.newWhichBranch = false;
                    sentInitData = false; // force re-request of create data.
                    break;
                }
            }
            if ((reinit || !sentInitData))
            {
                sentInitData = true;
                CMessageBuffer msg;
                msg.append(smt_initGraphReq);
                msg.append(graphId);
                assertex(!parentExtractSz || NULL!=parentExtract);
                msg.append(parentExtractSz);
                msg.append(parentExtractSz, parentExtract);
                if (!queryJob().queryJobComm().sendRecv(msg, 0, queryJob().querySlaveMpTag(), LONGTIMEOUT))
                    throwUnexpected();
                unsigned len;
                msg.read(len);
                if (len)
                    deserializeCreateContexts(msg);

// could still request 1 off, onCreate serialization from master 1st.
                CGraphBase::create(parentExtractSz, parentExtract);
                return;
            }
        }
    }
    CGraphBase::create(parentExtractSz, parentExtract);
}

void CSlaveGraph::abort(IException *e)
{
    if (!graphDone) // set pre done(), no need to abort if got that far.
        CGraphBase::abort(e);
    getDoneSem.signal();
}

void CSlaveGraph::done()
{
    GraphPrintLog("End of sub-graph");
    if (!aborted && (!queryOwner() || isGlobal()))
        getDoneSem.wait(); // must wait on master
    if (!queryOwner())
    {
        if (globals->getPropBool("@watchdogProgressEnabled"))
            jobS.queryProgressHandler()->stopGraph(*this, NULL);
    }

    Owned<IException> exception;
    try
    {
        CGraphBase::done();
    }
    catch (IException *e)
    {
        GraphPrintLog(e, NULL);
        exception.setown(e);
    }
    if (exception.get())
        throw LINK(exception.get());
}

void CSlaveGraph::end()
{
    CGraphBase::end();
    if (!queryOwner())
    {
        if (atomic_read(&nodesLoaded)) // wouldn't mean much if parallel jobs running
            GraphPrintLog("JHTree node stats:\ncacheAdds=%d\ncacheHits=%d\nnodesLoaded=%d\nblobCacheHits=%d\nblobCacheAdds=%d\nleafCacheHits=%d\nleafCacheAdds=%d\nnodeCacheHits=%d\nnodeCacheAdds=%d\n", atomic_read(&cacheAdds), atomic_read(&cacheHits), atomic_read(&nodesLoaded), atomic_read(&blobCacheHits), atomic_read(&blobCacheAdds), atomic_read(&leafCacheHits), atomic_read(&leafCacheAdds), atomic_read(&nodeCacheHits), atomic_read(&nodeCacheAdds));
        JSocketStatistics stats;
        getSocketStatistics(stats);
        StringBuffer s;
        getSocketStatisticsString(stats,s);
        GraphPrintLog("Socket statistics : %s\n",s.str());
        resetSocketStatistics();
    }
}

void CSlaveGraph::serializeStats(MemoryBuffer &mb)
{
    mb.append(queryGraphId());
    unsigned cPos = mb.length();
    unsigned count = 0;
    mb.append(count);
    CriticalBlock b(progressCrit);
    if (started || 0 == activityCount())
    {
        unsigned sPos = mb.length();
        Owned<IThorActivityIterator> iter = getTraverseIterator();
        ForEach (*iter)
        {
            if (mb.length() > (DATA_MAX-30))
            {
                WARNLOG("Act: Progress packet too big!");
                break;
            }
            
            CGraphElementBase &element = iter->query();
            CSlaveActivity &activity = (CSlaveActivity &)*element.queryActivity();
            unsigned pos = mb.length();
            mb.append(activity.queryContainer().queryId());
            activity.serializeStats(mb);
            if (pos == mb.length()-sizeof(activity_id))
                mb.rewrite(pos);
            else
                ++count;
        }
        mb.writeDirect(cPos, sizeof(count), &count);
        mb.append(queryChildGraphCount());
        Owned<IThorGraphIterator> childIter = getChildGraphs();
        ForEach(*childIter)
        {
            CSlaveGraph &graph = (CSlaveGraph &)childIter->query();
            graph.serializeStats(mb);
        }
    }
    else
        mb.append((unsigned)0); // sub graph count
}

void CSlaveGraph::serializeDone(MemoryBuffer &mb)
{
    mb.append(queryGraphId());
    unsigned cPos = mb.length();
    unsigned count=0;
    mb.append(count);
    Owned<IThorActivityIterator> iter = getTraverseIterator();
    ForEach (*iter)
    {
        CGraphElementBase &element = iter->query();
        if (element.queryActivity())
        {
            CSlaveActivity &activity = (CSlaveActivity &)*element.queryActivity();
            unsigned rPos = mb.length();
            mb.append(element.queryId());
            unsigned nl=0;
            mb.append(nl); // place holder for size of mb
            unsigned l = mb.length();
            activity.processDone(mb);
            nl = mb.length()-l;
            if (0 == nl)
                mb.rewrite(rPos);
            else
            {
                mb.writeDirect(l-sizeof(nl), sizeof(nl), &nl);
                ++count;
            }
        }
    }
    mb.writeDirect(cPos, sizeof(count), &count);
}

void CSlaveGraph::getDone(MemoryBuffer &doneInfoMb)
{
    if (!started) return;
    GraphPrintLog("Entering getDone");
    if (!queryOwner() || isGlobal())
    {
        try
        {
            serializeDone(doneInfoMb);
            if (!queryOwner())
            {
                if (globals->getPropBool("@watchdogProgressEnabled"))
                {
                    HeartBeatPacket hb;
                    jobS.queryProgressHandler()->stopGraph(*this, &hb);
                    doneInfoMb.append(hb.packetsize, &hb);
                }
            }
            doneInfoMb.append(job.queryMaxDiskUsage());
            queryJob().queryTimeReporter().serialize(doneInfoMb);
        }
        catch (IException *)
        {
            GraphPrintLog("Leaving getDone");
            getDoneSem.signal();
            throw;
        }
    }
    GraphPrintLog("Leaving getDone");
    getDoneSem.signal();
}

///////////////////////////

class CThorCodeContextSlave : public CThorCodeContextBase
{
    mptag_t mptag;
    Owned<IDistributedFileTransaction> superfiletransaction;

public:
    CThorCodeContextSlave(CJobBase &job, ILoadedDllEntry &querySo, IUserDescriptor &userDesc, mptag_t _mptag) : CThorCodeContextBase(job, querySo, userDesc), mptag(_mptag)
    {
    }
    virtual void setResultBool(const char *name, unsigned sequence, bool value) { throwUnexpected(); }
    virtual void setResultData(const char *name, unsigned sequence, int len, const void * data) { throwUnexpected(); }
    virtual void setResultDecimal(const char * stepname, unsigned sequence, int len, int precision, bool isSigned, const void *val) { throwUnexpected(); } 
    virtual void setResultInt(const char *name, unsigned sequence, __int64 value) { throwUnexpected(); }
    virtual void setResultRaw(const char *name, unsigned sequence, int len, const void * data) { throwUnexpected(); }
    virtual void setResultReal(const char * stepname, unsigned sequence, double value) { throwUnexpected(); }
    virtual void setResultSet(const char *name, unsigned sequence, bool isAll, size32_t len, const void * data, ISetToXmlTransformer * transformer) { throwUnexpected(); }
    virtual void setResultString(const char *name, unsigned sequence, int len, const char * str) { throwUnexpected(); }
    virtual void setResultUInt(const char *name, unsigned sequence, unsigned __int64 value) { throwUnexpected(); }
    virtual void setResultUnicode(const char *name, unsigned sequence, int len, UChar const * str) { throwUnexpected(); }
    virtual void setResultVarString(const char * name, unsigned sequence, const char * value) { throwUnexpected(); }
    virtual void setResultVarUnicode(const char * name, unsigned sequence, UChar const * value) { throwUnexpected(); }

    virtual bool getResultBool(const char * name, unsigned sequence) { throwUnexpected(); }
    virtual void getResultData(unsigned & tlen, void * & tgt, const char * name, unsigned sequence) { throwUnexpected(); }
    virtual void getResultDecimal(unsigned tlen, int precision, bool isSigned, void * tgt, const char * stepname, unsigned sequence) { throwUnexpected(); }
    virtual void getResultRaw(unsigned & tlen, void * & tgt, const char * name, unsigned sequence, IXmlToRowTransformer * xmlTransformer, ICsvToRowTransformer * csvTransformer) { throwUnexpected(); }
    virtual void getResultSet(bool & isAll, size32_t & tlen, void * & tgt, const char * name, unsigned sequence, IXmlToRowTransformer * xmlTransformer, ICsvToRowTransformer * csvTransformer) { throwUnexpected(); }
    virtual __int64 getResultInt(const char * name, unsigned sequence) { throwUnexpected(); }
    virtual double getResultReal(const char * name, unsigned sequence) { throwUnexpected(); }
    virtual void getResultString(unsigned & tlen, char * & tgt, const char * name, unsigned sequence) { throwUnexpected(); }
    virtual void getResultUnicode(unsigned & tlen, UChar * & tgt, const char * name, unsigned sequence) { throwUnexpected(); }
    virtual char *getResultVarString(const char * name, unsigned sequence) { throwUnexpected(); }
    virtual UChar *getResultVarUnicode(const char * name, unsigned sequence) { throwUnexpected(); }
    virtual unsigned getResultHash(const char * name, unsigned sequence) { throwUnexpected(); }

    virtual void getExternalResultRaw(unsigned & tlen, void * & tgt, const char * wuid, const char * stepname, unsigned sequence, IXmlToRowTransformer * xmlTransformer, ICsvToRowTransformer * csvTransformer) { throwUnexpected(); }

    virtual __int64 countDiskFile(const char * lfn, unsigned recordSize) { throwUnexpected(); } 
    virtual __int64 countIndex(__int64 activityId, IHThorCountIndexArg & arg) { throwUnexpected(); }
    virtual __int64 countDiskFile(__int64 id, IHThorCountFileArg & arg) { throwUnexpected(); }
    virtual void addWuException(const char * text, unsigned code, unsigned severity)
    {
        DBGLOG("%s", text);
        Owned<IThorException> e = MakeThorException(code, "%s", text);
        e->setAction(tea_warning);
        e->setOrigin("user");
        e->setAction(tea_warning);
        e->setSeverity((WUExceptionSeverity)severity);
        job.fireException(e);
    }
    virtual unsigned getNodes() { return job.queryJobGroup().ordinality()-1; }
    virtual unsigned getNodeNum() { return job.queryMyRank()-1; }
    virtual char *getFilePart(const char *logicalName, bool create=false)
    {
        CMessageBuffer msg;
        msg.append(smt_getPhysicalName);
        msg.append(logicalName);
        msg.append(getNodeNum());
        msg.append(create);
        if (!job.queryJobComm().sendRecv(msg, 0, mptag, LONGTIMEOUT))
            throwUnexpected();
        return (char *)msg.detach();
    }
    virtual unsigned __int64 getFileOffset(const char *logicalName)
    {
        CMessageBuffer msg;
        msg.append(smt_getFileOffset);
        if (!job.queryJobComm().sendRecv(msg, 0, mptag, LONGTIMEOUT))
            throwUnexpected();
        unsigned __int64 offset;
        msg.read(offset);
        return offset;
    }
    virtual IDistributedFileTransaction *querySuperFileTransaction()
    {
        // NB: shouldn't really have fileservice being called on slaves
        if (!superfiletransaction.get())
            superfiletransaction.setown(createDistributedFileTransaction(userDesc));
        return superfiletransaction.get();
    }
    virtual void getResultStringF(unsigned tlen, char * tgt, const char * name, unsigned sequence) { throwUnexpected(); }
    virtual void getResultRowset(size32_t & tcount, byte * * & tgt, const char * name, unsigned sequence, IEngineRowAllocator * _rowAllocator, IOutputRowDeserializer * deserializer, bool isGrouped, IXmlToRowTransformer * xmlTransformer, ICsvToRowTransformer * csvTransformer) { throwUnexpected(); }
    virtual void addWuAssertFailure(unsigned code, const char * text, const char * filename, unsigned lineno, unsigned column, bool isAbort)
    {
        DBGLOG("%s", text);
        Owned<IThorException> e = MakeThorException(code, "%s", text);
        e->setAssert(filename, lineno, column);
        e->setOrigin("user");
        e->setSeverity(ExceptionSeverityError);
        if (!isAbort)
            e->setAction(tea_warning);
        job.fireException(e);
    }
    virtual unsigned __int64 getDatasetHash(const char * name, unsigned __int64 hash)   { throwUnexpected(); }      // Should only call from master
};

class CSlaveGraphTempHandler : public CGraphTempHandler
{
public:
    CSlaveGraphTempHandler(CJobBase &job) : CGraphTempHandler(job)
    {
    }
    virtual bool removeTemp(const char *name)
    {
        OwnedIFile ifile = createIFile(name);
        return ifile->remove();
    }
};

#define SLAVEGRAPHPOOLLIMIT 10
CJobSlave::CJobSlave(ISlaveWatchdog *_watchdog, IPropertyTree *_workUnitInfo, const char *graphName, const char *_querySo, mptag_t _mpJobTag, mptag_t _slavemptag) : CJobBase(graphName), watchdog(_watchdog)
{
    workUnitInfo.set(_workUnitInfo);
    workUnitInfo->getProp("token", token);
    workUnitInfo->getProp("user", user);
    workUnitInfo->getProp("wuid", wuid);
    workUnitInfo->getProp("scope", scope);

    init();

    mpJobTag = _mpJobTag;
    slavemptag = _slavemptag;

    IPropertyTree *plugins = workUnitInfo->queryPropTree("plugins");
    if (plugins)
    {
        StringBuffer pluginsDir, installDir, pluginsList;
        globals->getProp("@INSTALL_DIR", installDir); // could use for socachedir also?
        if (installDir.length())
            addPathSepChar(installDir);
        globals->getProp("@pluginsPath", pluginsDir);
        if (pluginsDir.length())
        {
            if (!isAbsolutePath(pluginsDir.str())) // if !absolute, then make relative to installDir if is one (e.g. master mount)
            {
                if (installDir.length())
                    pluginsDir.insert(0, installDir.str());
            }
            addPathSepChar(pluginsDir);
        }
        Owned<IPropertyTreeIterator> pluginIter = plugins->getElements("plugin");
        ForEach(*pluginIter)
        {
            StringBuffer pluginPath;
            IPropertyTree &plugin = pluginIter->query();
            pluginPath.append(pluginsDir).append(plugin.queryProp("@name"));
            if (pluginsList.length())
                pluginsList.append(ENVSEPCHAR);
            pluginsList.append(pluginPath);
        }
        pluginMap->loadFromList(pluginsList.str());
    }
#ifdef __linux__
// only relevant if dllsToSlaves=false and query_so_dir was fully qualified remote path (e.g. //<ip>/path/file
    RemoteFilename rfn;
    rfn.setRemotePath(_querySo);
    StringBuffer tempSo;
    if (!rfn.isLocal())
    {
        WARNLOG("Cannot load shared object directly from remote path, creating temporary local copy: %s", _querySo);
        GetTempName(tempSo,"so",true);
        copyFile(tempSo.str(), _querySo);
        _querySo = tempSo.str();
    }
#endif
    querySo.setown(createDllEntry(_querySo, false, NULL));
    codeCtx = new CThorCodeContextSlave(*this, *querySo, *userDesc, slavemptag);
    tmpHandler.setown(new CSlaveGraphTempHandler(*this));
    startJob();
}

CJobSlave::~CJobSlave()
{
    graphExecutor->wait();
    endJob();
}

void CJobSlave::startJob()
{
    LOG(MCdebugProgress, thorJob, "New Graph started : %s", graphName.get());
    ClearTempDirs();
    unsigned pinterval = globals->getPropInt("@system_monitor_interval",1000*60);
    if (pinterval)
        startPerformanceMonitor(pinterval);
    if (pinterval) {
        perfmonhook.setown(createThorMemStatsPerfMonHook());
        startPerformanceMonitor(pinterval,PerfMonStandard,perfmonhook);
    }
    PrintMemoryStatusLog();
    unsigned __int64 freeSpace = getFreeSpace(queryBaseDirectory());
    unsigned __int64 freeSpaceRep = getFreeSpace(queryBaseDirectory(true));
    PROGLOG("Disk space: %s = %"I64F"d, %s = %"I64F"d", queryBaseDirectory(), freeSpace/0x100000, queryBaseDirectory(true), freeSpaceRep/0x100000);

    unsigned minFreeSpace = getWorkUnitValueInt("MINIMUM_DISK_SPACE", 0);
    if (minFreeSpace)
    {
        if (freeSpace < ((unsigned __int64)minFreeSpace)*0x100000)
        {
            SocketEndpoint ep;
            ep.setLocalHost(0);
            StringBuffer s;
            throw MakeThorException(TE_NotEnoughFreeSpace, "Node %s has %u MB(s) of available disk space, specified minimum for this job: %u MB(s)", ep.getUrlStr(s).str(), (unsigned) freeSpace / 0x100000, minFreeSpace);
        }
    }
    unsigned keyNodeCacheMB = (unsigned)getWorkUnitValueInt("keyNodeCacheMB", 0);
    if (keyNodeCacheMB)
    {
        setNodeCacheMem(keyNodeCacheMB * 0x100000);
        PROGLOG("Key node cache size set to: %d MB", keyNodeCacheMB);
    }
    unsigned keyFileCacheLimit = (unsigned)getWorkUnitValueInt("keyFileCacheLimit", 0);
    if (!keyFileCacheLimit)
        keyFileCacheLimit = (querySlaves()+1)*2;
    setKeyIndexCacheSize(keyFileCacheLimit);
    PROGLOG("Key file cache size set to: %d", keyFileCacheLimit);
}

void CJobSlave::endJob()
{
    stopPerformanceMonitor();
    LOG(MCdebugProgress, thorJob, "Job ended : %s", graphName.get());
    clearKeyStoreCache(true);
    PrintMemoryStatusLog();
}

__int64 CJobSlave::getWorkUnitValueInt(const char *prop, __int64 defVal) const
{
    StringBuffer propName(prop);
    return workUnitInfo->queryPropTree("Debug")->getPropInt64(propName.toLowerCase().str(), defVal);
}

StringBuffer &CJobSlave::getWorkUnitValue(const char *prop, StringBuffer &str) const
{
    workUnitInfo->queryPropTree("Debug")->getProp(prop, str);
    return str;
}

IBarrier *CJobSlave::createBarrier(mptag_t tag)
{
    return new CBarrierSlave(*jobComm, tag);
}

IGraphTempHandler *CJobSlave::createTempHandler()
{
    return new CSlaveGraphTempHandler(*this);
}

// IGraphCallback
void CJobSlave::runSubgraph(CGraphBase &graph, size32_t parentExtractSz, const byte *parentExtract)
{
    if (!graph.queryOwner())
        CJobBase::runSubgraph(graph, parentExtractSz, parentExtract);
    else
        graph.doExecuteChild(parentExtractSz, parentExtract);
    CriticalBlock b(graphRunCrit);
    if (!graph.queryOwner())
        removeSubGraph(graph);
}

///////////////

bool ensurePrimary(CActivityBase *activity, IPartDescriptor &partDesc, OwnedIFile & ifile, unsigned &location, StringBuffer &path)
{
    StringBuffer locationName, primaryName;
    RemoteFilename primaryRfn;
    partDesc.getFilename(0, primaryRfn);
    primaryRfn.getPath(primaryName);

    OwnedIFile primaryIFile = createIFile(primaryName.str());
    try
    {
        if (primaryIFile->exists())
        {
            location = 0;
            ifile.set(primaryIFile);
            path.append(primaryName);
            return true;
        }
    }
    catch (IException *e)
    {
        ActPrintLog(&activity->queryContainer(), e, NULL);
        e->Release();
    }
    unsigned l;
    for (l=1; l<partDesc.numCopies(); l++)
    {
        RemoteFilename altRfn;
        partDesc.getFilename(l, altRfn);
        locationName.clear();
        altRfn.getPath(locationName);
        assertex(locationName.length());
        OwnedIFile backupIFile = createIFile(locationName.str());
        try
        {
            if (backupIFile->exists())
            {
                if (primaryRfn.isLocal())
                {
                    ensureDirectoryForFile(primaryIFile->queryFilename());
                    Owned<IException> e = MakeActivityWarning(activity, 0, "Primary file missing: %s, copying backup %s to primary location", primaryIFile->queryFilename(), locationName.str());
                    activity->fireException(e);
                    StringBuffer tmpName(primaryIFile->queryFilename());
                    tmpName.append(".tmp");
                    OwnedIFile tmpFile = createIFile(tmpName.str());
                    CFIPScope fipScope(tmpName.str());
                    copyFile(tmpFile, backupIFile);
                    try
                    {
                        tmpFile->rename(pathTail(primaryIFile->queryFilename()));
                        location = 0;
                        ifile.set(primaryIFile);
                        path.append(primaryName);
                    }
                    catch (IException *e)
                    {
                        try { tmpFile->remove(); } catch (IException *e) { ActPrintLog(&activity->queryContainer(), "Failed to delete temporary file"); e->Release(); }
                        Owned<IException> e2 = MakeActivityWarning(activity, e, "Failed to restore primary, failed to rename %s to %s", tmpName.str(), primaryIFile->queryFilename());
                        e->Release();
                        activity->fireException(e2);
                        ifile.set(backupIFile);
                        location = l;
                        path.append(locationName);
                    }
                }
                else // JCSMORE - should use daliservix perhaps to ensure primary
                {
                    Owned<IException> e = MakeActivityWarning(activity, 0, "Primary file missing: %s, using remote copy: %s", primaryIFile->queryFilename(), locationName.str());
                    activity->fireException(e);
                    ifile.set(backupIFile);
                    location = l;
                    path.append(locationName);
                }
                return true;
            }
        }
        catch (IException *e)
        {
            Owned<IThorException> e2 = MakeActivityException(activity, e);
            e->Release();
            throw e2.getClear();
        }
    }
    return false;
}

class CEnsurePrimaryPartFile : public CInterface, implements IReplicatedFile
{
    CActivityBase &activity;
    Linked<IPartDescriptor> partDesc;
    StringAttr logicalFilename;
    Owned<IReplicatedFile> part;
public:
    IMPLEMENT_IINTERFACE;

    CEnsurePrimaryPartFile(CActivityBase &_activity, const char *_logicalFilename, IPartDescriptor *_partDesc) 
        : activity(_activity), logicalFilename(_logicalFilename), partDesc(_partDesc)
    {
    }
    virtual IFile *open()
    {
        unsigned location;
        OwnedIFile iFile;
        StringBuffer filePath;
        if (globals->getPropBool("@autoCopyBackup", true)?ensurePrimary(&activity, *partDesc, iFile, location, filePath):getBestFilePart(&activity, *partDesc, iFile, location, filePath, &activity))
            return iFile.getClear();
        else
        {
            StringBuffer locations;
            IException *e = MakeActivityException(&activity, TE_FileNotFound, "No physical file part for logical file %s, found at given locations: %s (Error = %d)", logicalFilename.get(), getFilePartLocations(*partDesc, locations).str(), GetLastError());
            EXCLOG(e, NULL);
            throw e;
        }
    }

    RemoteFilenameArray &queryCopies() 
    { 
        if(!part.get()) 
            part.setown(partDesc->getReplicatedFile());
        return part->queryCopies(); 
    }
};

IReplicatedFile *createEnsurePrimaryPartFile(CActivityBase &activity, const char *logicalFilename, IPartDescriptor *partDesc)
{
    return new CEnsurePrimaryPartFile(activity, logicalFilename, partDesc);
}

///////////////

class CFileCache;
class CLazyFileIO : public CInterface, implements IFileIO, implements IDelayedFile
{
    CFileCache &cache;
    Owned<IReplicatedFile> repFile;
    Linked<IExpander> expander;
    bool compressed;
    StringAttr filename;
    CriticalSection crit;
    Owned<IFileIO> iFileIO; // real IFileIO

    void checkOpen(); // references CFileCache method

public:
    IMPLEMENT_IINTERFACE;
    CLazyFileIO(CFileCache &_cache, const char *_filename, IReplicatedFile *_repFile, bool _compressed, IExpander *_expander) : cache(_cache), filename(_filename), repFile(_repFile), compressed(_compressed), expander(_expander)
    {
    }
    ~CLazyFileIO()
    {
        iFileIO.clear();
    }

    const char *queryFindString() const { return filename.get(); } // for string HT

// IFileIO impl.
    virtual size32_t read(offset_t pos, size32_t len, void * data)
    {
        CriticalBlock b(crit);
        checkOpen();
        return iFileIO->read(pos, len, data);
    }
    virtual offset_t size()
    {
        CriticalBlock b(crit);
        checkOpen();
        return iFileIO->size();
    }
    virtual size32_t write(offset_t pos, size32_t len, const void * data)
    {
        CriticalBlock b(crit);
        checkOpen();
        return iFileIO->write(pos, len, data);
    }
    virtual void flush()
    {
        CriticalBlock b(crit);
        if (iFileIO)
            iFileIO->flush();
    }
    virtual void close()
    {
        CriticalBlock b(crit);
        if (iFileIO)
            iFileIO->close();
        iFileIO.clear();
    }
    virtual offset_t appendFile(IFile *file,offset_t pos=0,offset_t len=(offset_t)-1)
    {
        CriticalBlock b(crit);
        checkOpen();
        return iFileIO->appendFile(file, pos, len);
    }
    virtual void setSize(offset_t size)
    {
        CriticalBlock b(crit);
        checkOpen();
        iFileIO->setSize(size);
    }
// IDelayedFile impl.
    virtual IMemoryMappedFile *queryMappedFile() { return NULL; }
    virtual IFileIO *queryFileIO() { return this; }
};

class CFileCache : public CInterface, implements IThorFileCache
{
    OwningStringSuperHashTableOf<CLazyFileIO> files;
    CopyCIArrayOf<CLazyFileIO> openFiles;
    unsigned limit, purgeN;
    CriticalSection crit;

    class CDelayedFileWapper : public CInterface, implements IDelayedFile
    {
        CFileCache &cache;
        Linked<CLazyFileIO> lFile;
    public:
        IMPLEMENT_IINTERFACE;

        CDelayedFileWapper(CFileCache &_cache, CLazyFileIO &_lFile) : cache(_cache), lFile(&_lFile) { }

        ~CDelayedFileWapper()
        {
            cache.remove(*lFile);
        }
        // IDelayedFile impl.
        virtual IMemoryMappedFile *queryMappedFile() { return lFile->queryMappedFile(); }
        virtual IFileIO *queryFileIO() { return lFile->queryFileIO(); }
    };

    void purgeOldest()
    {
        // will be ordered oldest first.
        unsigned count = 0;
        CopyCIArrayOf<CLazyFileIO> toClose;
        ForEachItemIn(o, openFiles)
        {
            CLazyFileIO &lFile = openFiles.item(o);
            toClose.append(lFile);
            if (++count>=purgeN) // crude for now, just remove oldest N
                break;
        }
        ForEachItemIn(r, toClose)
        {
            CLazyFileIO &lFile = toClose.item(r);
            lFile.close();
            openFiles.zap(lFile);
        }
    }
    bool _remove(CLazyFileIO &lFile)
    {
        bool ret = files.removeExact(&lFile);
        if (!ret) return false;
        openFiles.zap(lFile);
        return true;
    }
public:
    IMPLEMENT_IINTERFACE;

    CFileCache(unsigned _limit) : limit(_limit)
    {
        assertex(limit);
        purgeN = globals->getPropInt("@fileCachePurgeN", 10);
        if (purgeN > limit) purgeN=limit; // why would it be, but JIC.
        PROGLOG("FileCache: limit = %d, purgeN = %d", limit, purgeN);
    }

    void opening(CLazyFileIO &lFile)
    {
        CriticalBlock b(crit);
        if (openFiles.ordinality() >= limit)
        {
            purgeOldest(); // will close purgeN
            assertex(openFiles.ordinality() < limit);
        }
        openFiles.zap(lFile);
        openFiles.append(lFile);
    }

// IThorFileCache impl.
    virtual bool remove(IDelayedFile &dFile)
    {
        CLazyFileIO *lFile = QUERYINTERFACE(&dFile, CLazyFileIO);
        assertex(lFile);
        CriticalBlock b(crit);
        return _remove(*lFile);
    }
    virtual IDelayedFile *lookup(CActivityBase &activity, IPartDescriptor &partDesc, IExpander *expander)
    {
        StringBuffer filename;
        RemoteFilename rfn;
        partDesc.getFilename(0, rfn);
        rfn.getPath(filename);
        CriticalBlock b(crit);
        Linked<CLazyFileIO> file = files.find(filename.str());
        if (!file)
        {
            Owned<IReplicatedFile> repFile = createEnsurePrimaryPartFile(activity, filename.str(), &partDesc);
            bool compressed = partDesc.queryOwner().isCompressed();
            file.setown(new CLazyFileIO(*this, filename.str(), repFile.getClear(), compressed, expander));
        }
        files.replace(*LINK(file));
        return new CDelayedFileWapper(*this, *file); // to avoid circular dependency and allow destruction to remove from cache
    }
};
////
void CLazyFileIO::checkOpen()
{
    CriticalBlock b(crit);
    if (iFileIO)
        return;
    cache.opening(*this);
    Owned<IFile> iFile = repFile->open();
    if (NULL != expander.get())
        iFileIO.setown(createCompressedFileReader(iFile, expander));
    else if (compressed)
        iFileIO.setown(createCompressedFileReader(iFile));
    else
        iFileIO.setown(iFile->open(IFOread));
    if (!iFileIO.get())
        throw MakeThorException(0, "CLazyFileIO: failed to open: %s", filename.get());
}

IThorFileCache *createFileCache(unsigned limit)
{
    return new CFileCache(limit);
}
