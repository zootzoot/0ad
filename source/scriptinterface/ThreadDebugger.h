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
 
#ifndef THREADDEBUGGER_H_INCLUDED
#define THREADDEBUGGER_H_INCLUDED

#include "ScriptInterface.h"
#include "scriptinterface/ScriptExtraHeaders.h"
#include "DebuggingServer.h"

class CDebuggingServer;

// These Breakpoint classes are not implemented threadsafe. The class using the Breakpoints is responsible to make sure that 
// only one thread accesses the Breakpoint at a time
class CBreakPoint
{
public:
	CBreakPoint() { m_UserLine = 0; m_Filename = ""; }
	uint m_UserLine;
	std::string m_Filename;
};

// Only use this with one ScriptInterface/CThreadDebugger!
class CActiveBreakPoint : public CBreakPoint
{
public:
	CActiveBreakPoint() : 
		CBreakPoint()
	{
		Initialize();
	}

	CActiveBreakPoint(CBreakPoint breakPoint)
	{
		m_Filename = breakPoint.m_Filename;
		m_UserLine = breakPoint.m_UserLine;
		Initialize();
	}

	void Initialize()
	{
		m_ToRemove = false;
		m_Script = NULL;
		m_Pc = NULL;
		m_ActualLine = 0;
	}
	
	// the line where the breakpoint really is. As long has the breakpoint isn't set in spidermonkey this is the same as userLine.
	uint m_ActualLine;
	
	JSScript* m_Script;
	jsbytecode* m_Pc;

	// 
	bool m_ToRemove;
};

enum BREAK_SRC { BREAK_SRC_TRAP, BREAK_SRC_INTERRUP };


class CThreadDebugger
{
public:
	CThreadDebugger();
	~CThreadDebugger();

	/** @brief Initialize the object (required before using the object!).
	 *
	 * @param	id A unique identifier greater than 0 for the object inside its CDebuggingServer object.
	 * @param 	name A name that will be can be displayed by the UI to identify the thread.
	 * @param	pScriptInterface Pointer to a scriptinterface. All Hooks, breakpoint traps etc. will be registered in this 
	 *          scriptinterface and will be called by the thread this scriptinterface is running in.
	 * @param	pDebuggingServer Pointer to the DebuggingServer object this Object should belong to.
	 *
	 * @return	Return value.
	 */
	void Initialize(uint id, std::string name, ScriptInterface* pScriptInterface, CDebuggingServer* pDebuggingServer);
	
	JSTrapStatus TrapHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, jsval closure);
	
	/** @brief The callback function which is called when a trap (breakpoint, step etc.) is triggered by the scriptinterface's runtime.
	 *         This function needs to be public so we can call it from static function and pass a pointer to the CThreadDebugger object.
	 *         It is not supposed to be used outside this class even though it's public!
	 */
	JSTrapStatus BreakHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, jsval closure, BREAK_SRC breakSrc);
	
	JSTrapStatus StepHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure);
	JSTrapStatus StepIntoHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure);
	JSTrapStatus StepOutHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure);
	JSTrapStatus CheckForBreakRequestHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure);
	
	
	/** @brief The callback function which gets executed for each new script that gets loaded
	 *         (or actually each function inside a script plus the script as a whole).
	 *         This function needs to be public so we can call it from static function and pass a pointer to the CThreadDebugger object.
	 *         It is not supposed to be used outside this class even though it's public!
	 */
	void ExecuteHook(JSContext *cx, const char *filename, unsigned lineno, JSScript *script, JSFunction *fun, void *callerdata);
	
	void NewScriptHook(JSContext *cx, const char *filename, unsigned lineno, JSScript *script, JSFunction *fun, void *callerdata);
	void DestroyScriptHook(JSContext *cx, JSScript *script);

	void ClearTrap(CActiveBreakPoint* activeBreakPoint);
	
	/** @brief Checks if a mapping for the specified filename and line number exists in this CThreadDebugger's context
	 */	
	bool CheckIfMappingPresent(std::string filename, uint line);
	
	/** @brief Checks if a mapping exists for each breakpoint in the list of breakpoints that aren't set yet.
	 *         It there is a mapping, it removes the breakpoint from the list of unset breakpoints (from CDebuggingServer),
	 *         adds it to the list of active breakpoints (CThreadDebugger) and sets a trap.
	 *         Threading: m_Mutex is locked in this call
	 */	
	void SetAllNewTraps();
	
	/** @brief Sets a new trap and stores the information in the CActiveBreakPoint pointer
	 *         Make sure that a mapping exists before calling this function
	 *         Threading: Locking m_Mutex is required by the callee
	 */	
	void SetNewTrap(CActiveBreakPoint* activeBreakPoint, std::string filename, uint line);

	/** @brief Function used to inform the class about breakpoints set/unset on the server.
	 *         The server on the other hand is informed by the return value if the breakpoint is active in this object.
	 *         Threading: Locking m_Mutex is required by the callee
	 *
	 * @param	filename full vfs path to the script filename
	 * @param 	userLine linenumber where the USER set the breakpoint (UserLine)
	 *
	 * @return	true if the breakpoint registered in this object (independent of its state)
	 */
	bool ToggleBreakPoint(std::string filename, uint userLine);
	
	
	void GetCallstack(std::stringstream& response);
	void GetStackFrame(std::stringstream& response, uint nestingLevel);
	
	/** @brief Compares the object's associated scriptinterface and compares it with the pointer passed as parameter.
	 */
	bool CompareScriptInterfacePtr(ScriptInterface* pScriptInterface);
	
	// Getter/Setters for members that need to be threadsafe
	std::string GetBreakFileName();
	bool GetIsInBreak();
	uint GetLastBreakLine();
	void SetName(std::string name);
	std::string GetName();
	uint GetID();
	void ContinueExecution();
	void SetNextDbgCmd(DBGCMD dbgCmd);
	DBGCMD GetNextDbgCmd();
	
	
private:
	// Getters/Setters for members that need to be threadsafe
	void SetBreakFileName(std::string breakFileName);
	void SetLastBreakLine(uint breakLine);
	void SetIsInBreak(bool isInBreak);
	
	// Other threadsafe functions
	void SaveStackFrames();
	void SaveCallstack();
	
	
	CMutex m_Mutex;
	
	/// Used only in the scriptinterface's thread.
	void ClearTrapsToRemove();
	void ReturnActiveBreakPoints(jsbytecode* pBytecode);
	// This member could actually be used by other threads via CompareScriptInterfacePtr(), but that should be safe
	ScriptInterface* m_pScriptInterface; 
	CDebuggingServer* m_pDebuggingServer;
	JSStackFrame* m_pPrevStackFrame; // Required for step-out functionality
	// We store the pointer on the heap because the stack frame becomes invalid in certain cases
	// and spidermonkey throws errors if it detects a pointer on the stack.
	// We only use the pointer for comparing it with the current stack pointer and we don't try to access it, so it
	// shouldn't be a problem.
	JSStackFrame** m_pStackFrame;
	
	/// shared between multiple mongoose threads and one scriptinterface thread
	std::string m_BreakFileName;
	uint m_LastBreakLine;
	bool m_IsInBreak;
	DBGCMD m_NextDbgCmd;
	struct trapLocation
	{
		jsbytecode* pBytecode;
		JSScript* pScript;
		uint firstLineInFunction;
		uint lastLineInFunction;
	};
	std::map<std::string, std::map<uint, trapLocation> > m_LineToPCMap;
	
	//bool m_ContinueExecutionTriggered;
	//bool m_StepExecutionTriggered;

	std::list<CActiveBreakPoint*> m_ActiveBreakPoints;
	std::vector<std::string> m_StackFrames;
	std::string m_Callstack;
	
	/// shared between multiple mongoose threads (initialization may be an exception)
	std::string m_Name;	
	uint m_ID;
};

#endif // THREADDEBUGGER_H_INCLUDED
