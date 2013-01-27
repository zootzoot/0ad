/* Copyright (C) 2013 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include "precompiled.h"

#include "ps/CLogger.h"
#include "ThreadDebugger.h"



// Hooks
CMutex TrapHandlerMutex;
static JSTrapStatus TrapHandler_(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, jsval closure) 
{
	CScopeLock lock(TrapHandlerMutex);
	CThreadDebugger* pThreadDebugger = (CThreadDebugger*) JSVAL_TO_PRIVATE(closure);
	jsval val = JSVAL_NULL;
	return pThreadDebugger->TrapHandler(cx, script, pc, rval, val);
}

CMutex StepHandlerMutex;
JSTrapStatus StepHandler_(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure)
{
	CScopeLock lock(StepHandlerMutex);
	CThreadDebugger* pThreadDebugger = (CThreadDebugger*) closure;
	jsval val = JSVAL_VOID;
	return pThreadDebugger->StepHandler(cx, script, pc, rval, &val);
}

CMutex StepIntoHandlerMutex;
JSTrapStatus StepIntoHandler_(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure)
{
	CScopeLock lock(StepIntoHandlerMutex);
	CThreadDebugger* pThreadDebugger = (CThreadDebugger*) closure;
	return pThreadDebugger->StepIntoHandler(cx, script, pc, rval, NULL);
}

CMutex NewScriptHookMutex;
void NewScriptHook_(JSContext *cx, const char *filename, unsigned lineno, JSScript *script, JSFunction *fun, void *callerdata)
{
	CScopeLock lock(NewScriptHookMutex);
	CThreadDebugger* pThreadDebugger = (CThreadDebugger*) callerdata;
	return pThreadDebugger->NewScriptHook(cx, filename, lineno, script, fun, NULL);
}

CMutex DestroyScriptHookMutex;
void DestroyScriptHook_(JSContext *cx, JSScript *script, void *callerdata)
{
	CScopeLock lock(DestroyScriptHookMutex);
	CThreadDebugger* pThreadDebugger = (CThreadDebugger*) callerdata;
	return pThreadDebugger->DestroyScriptHook(cx, script);
}

CMutex StepOutHandlerMutex;
JSTrapStatus StepOutHandler_(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure)
{
	CScopeLock lock(StepOutHandlerMutex);
	CThreadDebugger* pThreadDebugger = (CThreadDebugger*) closure;
	return pThreadDebugger->StepOutHandler(cx, script, pc, rval, NULL);
}

CMutex CheckForBreakRequestHandlerMutex;
JSTrapStatus CheckForBreakRequestHandler_(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure)
{
	CScopeLock lock(CheckForBreakRequestHandlerMutex);
	CThreadDebugger* pThreadDebugger = (CThreadDebugger*) closure;
	return pThreadDebugger->CheckForBreakRequestHandler(cx, script, pc, rval, NULL);
}

CMutex CallHookMutex;
static void* CallHook_(JSContext* cx, JSStackFrame* fp, JSBool before, JSBool* ok, void* closure)
{
	CScopeLock lock(CallHookMutex);
	CThreadDebugger* pThreadDebugger = (CThreadDebugger*) closure;
	if (before)
	{
		JSScript* script;
		script = JS_GetFrameScript(cx, fp);
		const char* fileName = JS_GetScriptFilename(cx, script);
		uint lineno = JS_GetScriptBaseLineNumber(cx, script);
		JSFunction* fun = JS_GetFrameFunction(cx, fp);
		pThreadDebugger->ExecuteHook(cx, fileName, lineno, script, fun, closure);
	}
	
	return closure;
}

/// CThreadDebugger

void CThreadDebugger::ClearTrapsToRemove()
{
	CScopeLock lock(m_Mutex);
	std::list<CActiveBreakPoint*>::iterator itr=m_ActiveBreakPoints.begin();
	while(itr != m_ActiveBreakPoints.end())
	{
		if ((*itr)->m_ToRemove)
		{
				ClearTrap((*itr));
				// Remove the breakpoint
				delete (*itr);
				itr = m_ActiveBreakPoints.erase(itr);

		}
		else
			itr++;
	}
}

void CThreadDebugger::ClearTrap(CActiveBreakPoint* activeBreakPoint)
{
	ENSURE(activeBreakPoint->m_Script != NULL && activeBreakPoint->m_Pc != NULL);
	JSTrapHandler prevHandler;
	jsval prevClosure;	
	JS_ClearTrap(m_pScriptInterface->GetContext(), activeBreakPoint->m_Script, activeBreakPoint->m_Pc, &prevHandler, &prevClosure);
	LOGERROR(L"TRAP removed");
	activeBreakPoint->m_Script = NULL;
	activeBreakPoint->m_Pc = NULL;
}

void CThreadDebugger::SetAllNewTraps()
{
	std::list<CBreakPoint>* pBreakPoints = NULL;
	double breakPointsLockID;
	breakPointsLockID = m_pDebuggingServer->AquireBreakPointAccess(&pBreakPoints);
	std::list<CBreakPoint>::iterator itr = pBreakPoints->begin();
	while (itr != pBreakPoints->end())
	{
		if (CheckIfMappingPresent((*itr).m_Filename, (*itr).m_UserLine))
		{
			CActiveBreakPoint* pActiveBreakPoint = new CActiveBreakPoint((*itr));
			SetNewTrap(pActiveBreakPoint, (*itr).m_Filename, (*itr).m_UserLine);
			{
				CScopeLock lock(m_Mutex);
				m_ActiveBreakPoints.push_back(pActiveBreakPoint);
			}
			itr = pBreakPoints->erase(itr);
		}
		else
			itr++;
	}
	m_pDebuggingServer->ReleaseBreakPointAccess(breakPointsLockID);
}

bool CThreadDebugger::CheckIfMappingPresent(std::string filename, uint line)
{
	bool isPresent = (m_LineToPCMap.end() != m_LineToPCMap.find(filename) && m_LineToPCMap[filename].end() != m_LineToPCMap[filename].find(line));
	return isPresent;
}

void CThreadDebugger::SetNewTrap(CActiveBreakPoint* activeBreakPoint, std::string filename, uint line)
{
	ENSURE(activeBreakPoint->m_Script == NULL); // The trap must not be set already!
	ENSURE(CheckIfMappingPresent(filename, line)); // You have to check if the mapping exists before calling this function!

	jsbytecode* pc = m_LineToPCMap[filename][line].pBytecode;
	JSScript* script = m_LineToPCMap[filename][line].pScript;
	activeBreakPoint->m_Script = script;
	activeBreakPoint->m_Pc = pc;
	ENSURE(script != NULL && pc != NULL);
	// Not each line of code is a valid location for a breakpoint. 
	// The line returned here will be where the breakpoint was really set. 
	activeBreakPoint->m_ActualLine = JS_PCToLineNumber(m_pScriptInterface->GetContext(), script, pc);
	
	JS_SetTrap(m_pScriptInterface->GetContext(), script, pc, TrapHandler_, PRIVATE_TO_JSVAL(this));
	LOGERROR(L"Trap set filename: %hs Line: %d",  filename.c_str(), activeBreakPoint->m_ActualLine);
}


CThreadDebugger::CThreadDebugger()
{
	m_NextDbgCmd = DBG_CMD_NONE;
	m_IsInBreak = false;
	m_pStackFrame = new JSStackFrame*;
	m_pPrevStackFrame = NULL;
}

CThreadDebugger::~CThreadDebugger()
{
	LOGERROR(L"CThreadDebugger::~CThreadDebugger");
	
	// Clear all Traps and Breakpoints that are marked for removal
	ClearTrapsToRemove();
	
	// Return all breakpoints to the associated CDebuggingServer
	ReturnActiveBreakPoints(NULL);

	// Remove all the hooks because they store a pointer to this object
	JS_SetExecuteHook(m_pScriptInterface->GetRuntime(), NULL, NULL);
	JS_SetCallHook(m_pScriptInterface->GetRuntime(), NULL, NULL);
	JS_SetNewScriptHook(m_pScriptInterface->GetRuntime(), NULL, NULL);
	JS_SetDestroyScriptHook(m_pScriptInterface->GetRuntime(), NULL, NULL);
	
	delete m_pStackFrame;
}

void CThreadDebugger::ReturnActiveBreakPoints(jsbytecode* pBytecode)
{
	CScopeLock lock(m_Mutex);
	std::list<CActiveBreakPoint*>::iterator itr;
	itr = m_ActiveBreakPoints.begin();
	while (itr != m_ActiveBreakPoints.end())
	{
		// Breakpoints marked for removal should be deleted instead of returned!
		if ( ((*itr)->m_Pc == pBytecode || pBytecode == NULL) && !(*itr)->m_ToRemove)
		{
			std::list<CBreakPoint>* pBreakPoints;
			double breakPointsLockID = m_pDebuggingServer->AquireBreakPointAccess(&pBreakPoints);
			CBreakPoint breakPoint;
			breakPoint.m_UserLine = (*itr)->m_UserLine;
			breakPoint.m_Filename = (*itr)->m_Filename;
			// All active breakpoints should have a trap set
			ClearTrap((*itr));
			pBreakPoints->push_back(breakPoint);
			delete (*itr);
			itr=m_ActiveBreakPoints.erase(itr);
			m_pDebuggingServer->ReleaseBreakPointAccess(breakPointsLockID);
		}
		else
			itr++;
	}
}

void CThreadDebugger::Initialize(uint id, std::string name, ScriptInterface* pScriptInterface, CDebuggingServer* pDebuggingServer)
{
	LOGERROR(L"CThreadDebugger::Initialize");
	ENSURE(id != 0);
	m_ID = id;
	m_Name = name;
	m_pScriptInterface = pScriptInterface;
	m_pDebuggingServer = pDebuggingServer;
	JS_SetExecuteHook(m_pScriptInterface->GetRuntime(), CallHook_, (void*)this);
	JS_SetCallHook(m_pScriptInterface->GetRuntime(), CallHook_, (void*)this);
	JS_SetNewScriptHook(m_pScriptInterface->GetRuntime(), NewScriptHook_, (void*)this);
	JS_SetDestroyScriptHook(m_pScriptInterface->GetRuntime(), DestroyScriptHook_, (void*)this);
	
	if (m_pDebuggingServer->GetSettingSimultaneousThreadBreak())
	{
		// Setup a handler to check for break-requests from the DebuggingServer regularly
		JS_SetInterrupt(m_pScriptInterface->GetRuntime(), CheckForBreakRequestHandler_, this);
	}
}

JSTrapStatus CThreadDebugger::StepHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure)
{
	uint line = JS_PCToLineNumber(cx, script, pc);
	// We break when we are in the previous stack frame or in the same frame but on a different line.
	// On loops for example, we can go a few lines up again without leaving the current stack frame, so it's not necessarily 
	// a higher line number.
	JSStackFrame* iter = NULL;
	JSStackFrame* pStackFrame;
	pStackFrame = JS_FrameIterator(m_pScriptInterface->GetContext(), &iter);
	uint lastBreakLine = GetLastBreakLine() ;
	jsval val = JSVAL_VOID;
	if ((*m_pStackFrame == pStackFrame && lastBreakLine != line) || m_pPrevStackFrame == pStackFrame)
		return BreakHandler(cx, script, pc, rval, val, BREAK_SRC_INTERRUP);
	else
		return JSTRAP_CONTINUE;
}

JSTrapStatus CThreadDebugger::StepIntoHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure)
{
	uint line = JS_PCToLineNumber(cx, script, pc);
	// We break when we are on the same stack frame but not on the same line 
	// or when we are on another stack frame.
	JSStackFrame* iter = NULL;
	JSStackFrame* pStackFrame;
	pStackFrame = JS_FrameIterator(m_pScriptInterface->GetContext(), &iter);
	uint lastBreakLine = GetLastBreakLine();
	
	jsval val = JSVAL_VOID;
	if ((*m_pStackFrame == pStackFrame && lastBreakLine != line) || *m_pStackFrame != pStackFrame)
		return BreakHandler(cx, script, pc, rval, val, BREAK_SRC_INTERRUP);
	else
		return JSTRAP_CONTINUE;
}

JSTrapStatus CThreadDebugger::StepOutHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure)
{
	// We break when we are on the previous stack frame.
	JSStackFrame* iter = NULL;
	JSStackFrame* pStackFrame;
	pStackFrame = JS_FrameIterator(m_pScriptInterface->GetContext(), &iter);
	jsval val = JSVAL_VOID;
	if (m_pPrevStackFrame == pStackFrame)
		return BreakHandler(cx, script, pc, rval, val, BREAK_SRC_INTERRUP);
	else
		return JSTRAP_CONTINUE;
}

// Break this thread if the DebuggingServer requested it
JSTrapStatus CThreadDebugger::CheckForBreakRequestHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure)
{
	jsval val = JSVAL_VOID;
	if (m_pDebuggingServer->GetThreadsBreakRequested())
		return BreakHandler(cx, script, pc, rval, val, BREAK_SRC_INTERRUP);
	else
		return JSTRAP_CONTINUE;
}


JSTrapStatus CThreadDebugger::TrapHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, jsval closure)
{
	jsval val = JSVAL_NULL;
	return BreakHandler(cx, script, pc, rval, val, BREAK_SRC_TRAP);
}

JSTrapStatus CThreadDebugger::BreakHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, jsval closure, BREAK_SRC breakSrc) 
{
	LOGERROR(L"BreakHandler");
	
	uint line = JS_PCToLineNumber(cx, script, pc);
	std::string filename(JS_GetScriptFilename(cx, script));

	SetIsInBreak(true);
	SaveCallstack();
	SaveStackFrames();
	SetLastBreakLine(line);
	SetBreakFileName(filename);
	*m_pStackFrame = NULL;
	m_pPrevStackFrame = NULL;
	
	if (breakSrc == BREAK_SRC_INTERRUP)
	{
		JS_ClearInterrupt(m_pScriptInterface->GetRuntime(), NULL, NULL);
		JS_SetSingleStepMode(cx, script, false);
	}
	
	if (m_pDebuggingServer->GetSettingSimultaneousThreadBreak())
	{
		m_pDebuggingServer->RequestThreadsToBreak(true);
	}
	
	// Wait until the user continues the execution
	while(1)
	{
		DBGCMD nextDbgCmd = GetNextDbgCmd();
		if (nextDbgCmd == DBG_CMD_NONE)
		{
			// Wait a while before checking for new m_NextDbgCmd again.
			// We don't want this loop to take 100% of a CPU core for each thread that is in break mode.
			// On the other hande we don't want the debugger to become unresponsive.
			SDL_Delay(100);	
		}
		else if (nextDbgCmd == DBG_CMD_SINGLESTEP || nextDbgCmd == DBG_CMD_STEPINTO || nextDbgCmd == DBG_CMD_STEPOUT)
		{
			JSStackFrame* iter = NULL;
			*m_pStackFrame = JS_FrameIterator(m_pScriptInterface->GetContext(), &iter);
			m_pPrevStackFrame = JS_FrameIterator(m_pScriptInterface->GetContext(), &iter);
			
			if (!JS_SetSingleStepMode(cx, script, true))
				LOGERROR(L"JS_SetSingleStepMode returned false!"); // TODO: When can this happen?
			else
			{
				if (nextDbgCmd == DBG_CMD_SINGLESTEP)
				{
					JS_SetInterrupt(m_pScriptInterface->GetRuntime(), StepHandler_, this);
					break;
				}
				else if (nextDbgCmd == DBG_CMD_STEPINTO)
				{
					JS_SetInterrupt(m_pScriptInterface->GetRuntime(), StepIntoHandler_, this);
					break;
				}
				else if (nextDbgCmd == DBG_CMD_STEPOUT)
				{
					JS_SetInterrupt(m_pScriptInterface->GetRuntime(), StepOutHandler_, this);
					break;
				}
			}
		}
		else if(nextDbgCmd == DBG_CMD_CONTINUE)
		{
			if (m_pDebuggingServer->GetSettingSimultaneousThreadBreak())
			{
				if (!JS_SetSingleStepMode(cx, script, true))
					LOGERROR(L"JS_SetSingleStepMode returned false!"); // TODO: When can this happen?
				else
				{
					// Setup a handler to check for break-requests from the DebuggingServer regularly
					JS_SetInterrupt(m_pScriptInterface->GetRuntime(), CheckForBreakRequestHandler_, this);
				}
			}
			break;
		}
		else 
			debug_warn("Invalid DBGCMD found in CThreadDebugger::BreakHandler!");
	}
	ClearTrapsToRemove();
	SetAllNewTraps();
	SetNextDbgCmd(DBG_CMD_NONE);
	SetIsInBreak(false);
	SetBreakFileName("");
	
	//	JSTRAP_ERROR,
	//    JSTRAP_CONTINUE,
	//    JSTRAP_RETURN,
	//    JSTRAP_THROW,
	//    JSTRAP_LIMIT
	return JSTRAP_CONTINUE;
}

void CThreadDebugger::NewScriptHook(JSContext *cx, const char *filename, unsigned lineno, JSScript *script, JSFunction *fun, void *callerdata)
{
	uint scriptExtent = JS_GetScriptLineExtent (cx, script);
    std::string stringFileName(filename);
    if (stringFileName == "")
    	return;
    	
//	if (stringFileName != "(eval)")
//	{
//		LOGERROR(L"NewScriptHook: %s", stringFileName.c_str());
//	}
	

	for (uint line = lineno; line < scriptExtent + lineno; ++line) 
	{
		// If we already have a mapping for this line, we check if the current scipt is more deeply nested.
		// It it isn't more deeply nested, we don't overwrite the previous mapping
		// The most deeply nested script is always the one that must be used!
		uint firstLine = 0;
		uint lastLine = 0;
		jsbytecode* oldPC = NULL;
		if (CheckIfMappingPresent(stringFileName, line))
		{
			firstLine = m_LineToPCMap[stringFileName][line].firstLineInFunction;
			lastLine = m_LineToPCMap[stringFileName][line].lastLineInFunction;
			
			// If an entry nested equally is present too, we must overwrite it.
			// The same script(function) can trigger a NewScriptHook multiple times without DestroyScriptHooks between these 
			// calls. In this case the old script becomes invalid.
			if (lineno < firstLine || scriptExtent + lineno > lastLine)
				continue;
			else
				oldPC = m_LineToPCMap[stringFileName][line].pBytecode;
				
		}
		jsbytecode* pc = JS_LineNumberToPC (cx, script, line);
		m_LineToPCMap[stringFileName][line].pBytecode = pc;
		m_LineToPCMap[stringFileName][line].pScript = script;
		m_LineToPCMap[stringFileName][line].firstLineInFunction = lineno;
		m_LineToPCMap[stringFileName][line].lastLineInFunction = lineno + scriptExtent;
		
		// If we are replacing a script, the associated traps become invalid
		if (lineno == firstLine && scriptExtent + lineno == lastLine)
		{
			ReturnActiveBreakPoints(oldPC);
			SetAllNewTraps();
		}
	}
}

void CThreadDebugger::DestroyScriptHook(JSContext *cx, JSScript *script)
{
	uint scriptExtent = JS_GetScriptLineExtent (cx, script);
	uint baseLine = JS_GetScriptBaseLineNumber(cx, script);
	
	char* pStr = NULL;
	pStr = (char*)JS_GetScriptFilename(cx, script);
	if(pStr != NULL)
	{
		std::string fileName(pStr);
//		if(fileName != "(eval)")
//			LOGERROR(L"DestroyScriptHook: %s", fileName.c_str());

		for (uint line = baseLine; line < scriptExtent + baseLine; ++line) 
		{
			if (CheckIfMappingPresent(fileName, line))
			{
				if (m_LineToPCMap[fileName][line].pScript == script)
				{
					ReturnActiveBreakPoints(m_LineToPCMap[fileName][line].pBytecode);
					m_LineToPCMap[fileName].erase(line);
					if (m_LineToPCMap[fileName].empty())
						m_LineToPCMap.erase(fileName);
				}
			}
		}
	}
}

void CThreadDebugger::ExecuteHook(JSContext *cx, const char *filename, unsigned lineno, JSScript *script, JSFunction *fun, void *callerdata)
{
	// Search all breakpoints that have no trap set yet
	{
		PROFILE2("ExecuteHook");
		SetAllNewTraps();
	}
	return;
}

bool CThreadDebugger::ToggleBreakPoint(std::string filename, uint userLine)
{
	LOGERROR(L"CThreadDebugger::ToggleBreakPoint");
	CScopeLock lock(m_Mutex);
	std::list<CActiveBreakPoint*>::iterator itr;
	for (itr=m_ActiveBreakPoints.begin(); itr != m_ActiveBreakPoints.end(); itr++)
	{
		if ((*itr)->m_UserLine == userLine && (*itr)->m_Filename == filename)
		{
			(*itr)->m_ToRemove = !(*itr)->m_ToRemove;
			return true;
		}
	}
	return false;
}

void CThreadDebugger::GetCallstack(std::stringstream& response)
{
	CScopeLock lock(m_Mutex);
	response << m_Callstack;
}

void CThreadDebugger::SaveCallstack()
{
	LOGERROR(L"CThreadDebugger::SaveCallstack");
	ENSURE(GetIsInBreak());
	
	CScopeLock lock(m_Mutex);
	
	JSStackFrame *fp;	
	JSStackFrame *iter = 0;
	std::string functionName;
	jsint counter = 0;
	
	JSObject* jsArray;
	jsArray = JS_NewArrayObject(m_pScriptInterface->GetContext(), 0, 0);
	JSString* functionID;

	fp = JS_FrameIterator(m_pScriptInterface->GetContext(), &iter);

	while(fp)
	{
		JSFunction* fun = 0;
		fun = JS_GetFrameFunction(m_pScriptInterface->GetContext(), fp);
		if (NULL == fun)
			functionID = JS_NewStringCopyZ(m_pScriptInterface->GetContext(), "null");
		else
		{
			functionID = JS_GetFunctionId(fun);
			if (NULL == functionID)
				functionID = JS_NewStringCopyZ(m_pScriptInterface->GetContext(), "anonymous");
		}

		ENSURE(JS_DefineElement(m_pScriptInterface->GetContext(), jsArray, counter, STRING_TO_JSVAL(functionID), NULL, NULL, 0));
		fp = JS_FrameIterator(m_pScriptInterface->GetContext(), &iter);
		counter++;
	}
	
	m_Callstack.clear();
	m_Callstack = m_pScriptInterface->StringifyJSON(OBJECT_TO_JSVAL(jsArray), false).c_str();
}

void CThreadDebugger::GetStackFrame(std::stringstream& response, uint nestingLevel)
{
	CScopeLock lock(m_Mutex);
	response.clear();
	if (m_StackFrames.size()-1 >= nestingLevel)
		response << m_StackFrames[nestingLevel];
}

void CThreadDebugger::SaveStackFrames()
{
	LOGERROR(L"CThreadDebugger::SaveStackFrames");
	ENSURE(GetIsInBreak());
	
	CScopeLock lock(m_Mutex);
	JSStackFrame *fp;	
	JSStackFrame *iter = 0;
	uint counter = 0;
	m_StackFrames.clear();
	jsval val;
	
	fp = JS_FrameIterator(m_pScriptInterface->GetContext(), &iter);
	while(fp)
	{
		if(JS_GetFrameThis(m_pScriptInterface->GetContext(), fp, &val))
		{
			m_StackFrames.push_back(m_pScriptInterface->StringifyJSON(val, false));
		}
		else
			m_StackFrames.push_back("");
		
		counter++;
		fp = JS_FrameIterator(m_pScriptInterface->GetContext(), &iter);
	}
}

bool CThreadDebugger::CompareScriptInterfacePtr(ScriptInterface* pScriptInterface)
{
	return (pScriptInterface == m_pScriptInterface);
}


std::string CThreadDebugger::GetBreakFileName()
{
	CScopeLock lock(m_Mutex);
	return m_BreakFileName;
}

void CThreadDebugger::SetBreakFileName(std::string breakFileName)
{
	CScopeLock lock(m_Mutex);
	m_BreakFileName = breakFileName;
}

uint CThreadDebugger::GetLastBreakLine()
{
	CScopeLock lock(m_Mutex);
	return m_LastBreakLine;
}

void CThreadDebugger::SetLastBreakLine(uint breakLine)
{
	CScopeLock lock(m_Mutex);
	m_LastBreakLine = breakLine;
}

bool CThreadDebugger::GetIsInBreak()
{
	CScopeLock lock(m_Mutex);
	return m_IsInBreak;
}

void CThreadDebugger::SetIsInBreak(bool isInBreak)
{
	CScopeLock lock(m_Mutex);
	m_IsInBreak = isInBreak;
}

void CThreadDebugger::SetNextDbgCmd(DBGCMD dbgCmd)
{
	CScopeLock lock(m_Mutex);
		m_NextDbgCmd = dbgCmd;
}

DBGCMD CThreadDebugger::GetNextDbgCmd()
{
	CScopeLock lock(m_Mutex);
	return m_NextDbgCmd;
}



void CThreadDebugger::SetName(std::string name)
{
	CScopeLock lock(m_Mutex);
	m_Name = name;
}

std::string CThreadDebugger::GetName()
{
	CScopeLock lock(m_Mutex);
	return m_Name;
}

uint CThreadDebugger::GetID()
{
	CScopeLock lock(m_Mutex);
	return m_ID;
}

