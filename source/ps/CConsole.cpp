#include "precompiled.h"
#include <wctype.h>

#include "CConsole.h"

#include "lib/lib.h"
#include "lib/ogl.h"
#include "lib/res/file/vfs.h"
#include "lib/res/graphics/unifont.h"
#include "lib/sysdep/sysdep.h"
#include "maths/MathUtil.h"
#include "ps/CLogger.h"
#include "ps/Globals.h"
#include "ps/Hotkey.h"
#include "ps/Interact.h"
#include "network/Client.h"
#include "network/Server.h"
#include "ps/Pyrogenesis.h"
#include "scripting/ScriptingHost.h"
#include "simulation/Entity.h"

CConsole* g_Console = 0;

CConsole::CConsole()
{

	m_bToggle = false;
	m_bVisible = false;

	m_fVisibleFrac = 0.0f;

	m_szBuffer = new wchar_t[CONSOLE_BUFFER_SIZE];
	FlushBuffer();

	m_iMsgHistPos = 1;
	m_charsPerPage=0;

	InsertMessage(L"[ 0 A.D. Console v0.12 ]   type \"\\info\" for help");
	InsertMessage(L"");

	if (vfs_exists("gui/text/help.txt"))
	{
		FileIOBuf buf;
		size_t size;
		if ( vfs_load("gui/text/help.txt", buf, size) < 0 )
		{
			LOG( ERROR,"Console", "Help file not found for console" );
			file_buf_free(buf);
			return;
		}
		// TODO: read in text mode, or at least get rid of the \r\n somehow
		// TODO: maybe the help file should be UTF-8 - we assume it's iso-8859-1 here
		m_helpText = CStrW(CStr( (const char*)buf ));
		file_buf_free(buf);
	}
	else
	{
		InsertMessage(L"No help file found.");
	}
}

CConsole::~CConsole()
{
	m_mapFuncList.clear();
	m_deqMsgHistory.clear();
	m_deqBufHistory.clear();
	delete[] m_szBuffer;
}


void CConsole::SetSize(float X, float Y, float W, float H)
{
	m_fX = X;
	m_fY = Y;
	m_fWidth = W;
	m_fHeight = H;
}

void CConsole::UpdateScreenSize(int w, int h)
{
	float height = h * 0.6f;
	SetSize(0, h-height, (float)w, height);
}


void CConsole::ToggleVisible()
{
	m_bToggle = true;
	m_bVisible = !m_bVisible;
}

void CConsole::SetVisible( bool visible )
{
	if( visible != m_bVisible )
		m_bToggle = true;
	m_bVisible = visible;
}

void CConsole::FlushBuffer(void)
{
	// Clear the buffer and set the cursor and length to 0
	memset(m_szBuffer, '\0', sizeof(wchar_t) * CONSOLE_BUFFER_SIZE);
	m_iBufferPos = m_iBufferLength = 0;
}


void CConsole::ToLower(wchar_t* szMessage, uint iSize)
{
	uint L = (uint)wcslen(szMessage);

	if (L <= 0) return;

	if (iSize && iSize < L) L = iSize;

	for(uint i = 0; i < L; i++)
		szMessage[i] = towlower(szMessage[i]);
}


void CConsole::Trim(wchar_t* szMessage, const wchar_t cChar, uint iSize)
{
	size_t L = wcslen(szMessage);
	if(!L)
		return;

	if (iSize && iSize < L) L = iSize;

	wchar_t szChar[2] = { cChar, 0 };

	// Find the first point at which szChar does not
	// exist in the message
	size_t ofs = wcsspn(szMessage, szChar);
	if(ofs == 0)	// no leading <cChar> chars - we're done
		return;

	// move everything <ofs> chars left, replacing leading cChar chars
	L -= ofs;
	memmove(szMessage, szMessage+ofs, L*sizeof(wchar_t));

	for(ssize_t i = (ssize_t)L; i >= 0; i--)
	{
		szMessage[i] = '\0';
		if (szMessage[i - 1] != cChar) break;
	}
}


void CConsole::RegisterFunc(fptr F, const wchar_t* szName)
{
	// need to allocate a copy - szName may be a const string literal
	// (we'll change it - stripping out spaces and converting to lowercase).
	wchar_t copy[CONSOLE_BUFFER_SIZE];
	copy[CONSOLE_BUFFER_SIZE-1] = '\0';
	wcsncpy(copy, szName, CONSOLE_BUFFER_SIZE-1);

	Trim(copy);
	ToLower(copy);

	m_mapFuncList.insert(std::pair<std::wstring, fptr>(copy, F));
}


void CConsole::Update(const float DeltaTime)
{
	if(m_bToggle)
	{
		const float AnimateTime = .30f;
		const float Delta = DeltaTime / AnimateTime;
		if(m_bVisible)
		{
			m_fVisibleFrac += Delta;
			if(m_fVisibleFrac > 1.0f)
			{
				m_fVisibleFrac = 1.0f;
				m_bToggle = false;
			}
		}
		else
		{
			m_fVisibleFrac -= Delta;
			if(m_fVisibleFrac < 0.0f)
			{
				m_fVisibleFrac = 0.0f;
				m_bToggle = false;
			}
		}
	}
}

//Render Manager.
void CConsole::Render()
{
	if (! (m_bVisible || m_bToggle) ) return;

	// animation: slide in from top of screen
	const float MaxY = m_fHeight;
	const float DeltaY = (1.0f - m_fVisibleFrac) * MaxY;

	glTranslatef(m_fX, m_fY + DeltaY, 0.0f); //Move to window position

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	DrawWindow();
	DrawHistory();
	DrawBuffer();

	glDisable(GL_BLEND);
}


void CConsole::DrawWindow(void)
{
	// TODO:  Add texturing
	glDisable(GL_TEXTURE_2D);

	glPushMatrix();

		// Draw Background
		// Set the color to a translucent blue
		glColor4f(0.0f, 0.0f, 0.5f, 0.6f);
		glBegin(GL_QUADS);
			glVertex2f(0.0f,		0.0f);
			glVertex2f(m_fWidth-1.0f,	0.0f);
			glVertex2f(m_fWidth-1.0f,	m_fHeight-1.0f);
			glVertex2f(0.0f,		m_fHeight-1.0f);
		glEnd();

		// Draw Border
		// Set the color to a translucent yellow
		glColor4f(0.5f, 0.5f, 0.0f, 0.6f);
		glBegin(GL_LINE_LOOP);
			glVertex2f(0.0f,		0.0f);
			glVertex2f(m_fWidth-1.0f,	0.0f);
			glVertex2f(m_fWidth-1.0f,	m_fHeight-1.0f);
			glVertex2f(0.0f,		m_fHeight-1.0f);
		glEnd();

		if (m_fHeight > m_iFontHeight + 4)
		{
			glBegin(GL_LINES);
				glVertex2f(0.0f,		(GLfloat)(m_iFontHeight + 4));
				glVertex2f(m_fWidth,	(GLfloat)(m_iFontHeight + 4));
			glEnd();
		}
	glPopMatrix();

	glEnable(GL_TEXTURE_2D);
}


void CConsole::DrawHistory(void) {
	int i = 1;

	std::deque<std::wstring>::iterator Iter; //History iterator

	glPushMatrix();
		glColor3f(1.0f, 1.0f, 1.0f); //Set color of text
		glTranslatef(9.0f, (float)m_iFontOffset, 0.0f); //move away from the border

		// Draw the text upside-down, because it's aligned with
		// the GUI (which uses the top-left as (0,0))
		glScalef(1.0f, -1.0f, 1.0f);

		for (Iter = m_deqMsgHistory.begin();
			 Iter != m_deqMsgHistory.end()
				 && (((i - m_iMsgHistPos + 1) * m_iFontHeight) < m_fHeight);
			 Iter++)
		{
			if (i >= m_iMsgHistPos){
				glTranslatef(0.0f, -(float)m_iFontHeight, 0.0f);

				glPushMatrix();
					glwprintf(L"%ls", Iter->data());
				glPopMatrix();
			}

			i++;
		}
	glPopMatrix();
}

//Renders the buffer to the screen.
void CConsole::DrawBuffer(void)
{
	if (m_fHeight < m_iFontHeight) return;

	glPushMatrix();
		glColor3f(1.0f, 1.0f, 0.0f);
		glTranslatef(2.0f, (float)m_iFontOffset, 0);
		glScalef(1.0f, -1.0f, 1.0f);

		glwprintf(L"]");

		glColor3f(1.0f, 1.0f, 1.0f);
		if (m_iBufferPos==0) DrawCursor();

		for (int i = 0; i < m_iBufferLength; i++){
				glwprintf(L"%lc", m_szBuffer[i]);
				if (m_iBufferPos-1==i) DrawCursor();
		}
	glPopMatrix();
}


void CConsole::DrawCursor(void)
{
	// (glPushMatrix is necessary because glwprintf does glTranslatef)
	glPushMatrix();
		// Slightly translucent yellow
		glColor4f(1.0f, 1.0f, 0.0f, 0.8f);

		// U+FE33: PRESENTATION FORM FOR VERTICAL LOW LINE
		// (sort of like a | which is aligned to the left of most characters)
		glwprintf(L"%lc", 0xFE33);

		// Revert to the standard text colour
		glColor3f(1.0f, 1.0f, 1.0f);
	glPopMatrix();
}


//Inserts a character into the buffer.
void CConsole::InsertChar(const int szChar, const wchar_t cooked )
{
	static int iHistoryPos = -1;

	if (!m_bVisible) return;

	switch (szChar){
		case SDLK_RETURN:
			iHistoryPos = -1;
			m_iMsgHistPos = 1;
			ProcessBuffer(m_szBuffer);
			FlushBuffer();
			return;

		case SDLK_TAB:
			// Auto Complete
			return;

		case SDLK_BACKSPACE:
			if (IsEmpty() || IsBOB()) return;

			if (m_iBufferPos == m_iBufferLength)
				m_szBuffer[m_iBufferPos - 1] = '\0';
			else{
				for(int j=m_iBufferPos-1; j<m_iBufferLength-1; j++)
					m_szBuffer[j] = m_szBuffer[j+1]; // move chars to left
				m_szBuffer[m_iBufferLength-1] = '\0';
			}

			m_iBufferPos--;
			m_iBufferLength--;
			return;

		case SDLK_DELETE:
			if (IsEmpty() || IsEOB()) return;

			if (m_iBufferPos == m_iBufferLength-1)
			{
				m_szBuffer[m_iBufferPos] = '\0';
				m_iBufferLength--;
			}
			else
			{
				if (g_keys[SDLK_RCTRL] || g_keys[SDLK_LCTRL])
				{
					// Make Ctrl-Delete delete up to end of line
					m_szBuffer[m_iBufferPos] = '\0';
					m_iBufferLength = m_iBufferPos;
				}
				else 
				{
					// Delete just one char and move the others left
					for(int j=m_iBufferPos; j<m_iBufferLength-1; j++)
						m_szBuffer[j] = m_szBuffer[j+1];
					m_szBuffer[m_iBufferLength-1] = '\0';
					m_iBufferLength--;
				}
			}

			return;

		case SDLK_HOME:
			if (g_keys[SDLK_RCTRL] || g_keys[SDLK_LCTRL])
			{
				int linesShown = (int)m_fHeight/m_iFontHeight - 4;
				m_iMsgHistPos = clamp((int)m_deqMsgHistory.size() - linesShown, 1, (int)m_deqMsgHistory.size());
			}
			else
			{
				m_iBufferPos = 0;
			}
			return;

		case SDLK_END:
			if (g_keys[SDLK_RCTRL] || g_keys[SDLK_LCTRL])
			{
				m_iMsgHistPos = 1;
			}
			else
			{
				m_iBufferPos = m_iBufferLength;
			}
			return;

		case SDLK_LEFT:
			if (m_iBufferPos) m_iBufferPos--;
			return;

		case SDLK_RIGHT:
			if (m_iBufferPos != m_iBufferLength) m_iBufferPos++;
			return;

		// BEGIN: Buffer History Lookup
		case SDLK_UP:
			if ( m_deqBufHistory.size() )
			{
				int oldHistoryPos = iHistoryPos;
				while( iHistoryPos != (int)m_deqBufHistory.size() - 1)
				{
					iHistoryPos++;
					std::wstring& histString = m_deqBufHistory.at(iHistoryPos);
					if((int)histString.length() >= m_iBufferPos)
					{
						bool bad = false;
						for(int i=0; i<m_iBufferPos; i++)
						{
							if(histString[i] != m_szBuffer[i])
							{
								bad = true; break;
							}
						}
						if(!bad)
						{
							SetBuffer(m_deqBufHistory.at(iHistoryPos).data());
							return;
						}
					}
				}
				// if we got here, failed to find a string with the right prefix;
				// revert to the old position in case the user types more stuff
				iHistoryPos = oldHistoryPos;
			}
			return;

		case SDLK_DOWN:
			if ( m_deqBufHistory.size() && iHistoryPos > 0 )
			{
				int oldHistoryPos = iHistoryPos;
				while( iHistoryPos != 0)
				{
					iHistoryPos--;
					std::wstring& histString = m_deqBufHistory.at(iHistoryPos);
					if((int)histString.length() >= m_iBufferPos)
					{
						bool bad = false;
						for(int i=0; i<m_iBufferPos; i++)
						{
							if(histString[i] != m_szBuffer[i])
							{
								bad = true; break;
							}
						}
						if(!bad)
						{
							SetBuffer(m_deqBufHistory.at(iHistoryPos).data());
							return;
						}
					}
				}
				// if we got here, failed to find a string with the right prefix;
				// revert to the old position in case the user types more stuff,
				// and also clear any complietion we might've added going up
				iHistoryPos = oldHistoryPos;
				m_szBuffer[m_iBufferPos] = 0;
				m_iBufferLength = m_iBufferPos;
			}
			return;
		// END: Buffer History Lookup

		// BEGIN: Message History Lookup
		case SDLK_PAGEUP:
			if (m_iMsgHistPos != (int)m_deqMsgHistory.size()) m_iMsgHistPos++;
			return;

		case SDLK_PAGEDOWN:
			if (m_iMsgHistPos != 1) m_iMsgHistPos--;
			return;
		// END: Message History Lookup

		default: //Insert a character
			if (IsFull()) return;
			if (cooked == 0) return;

			if (IsEOB()) //are we at the end of the buffer?
				m_szBuffer[m_iBufferPos] = cooked; //cat char onto end
			else{ //we need to insert
				int i;
				for(i=m_iBufferLength; i>m_iBufferPos; i--)
					m_szBuffer[i] = m_szBuffer[i-1]; // move chars to right
				m_szBuffer[i] = cooked;
			}

			m_iBufferPos++;
			m_iBufferLength++;

			return;
	}
}


void CConsole::InsertMessage(const wchar_t* szMessage, ...)
{
	va_list args;
	wchar_t szBuffer[CONSOLE_MESSAGE_SIZE];

	va_start(args, szMessage);
	if (vswprintf(szBuffer, CONSOLE_MESSAGE_SIZE, szMessage, args) == -1)
	{
		debug_printf("Error printfing console message (buffer size exceeded?)\n");

		// Make it obvious that the text was trimmed (assuming it was)
		wcscpy(szBuffer+CONSOLE_MESSAGE_SIZE-4, L"...");
	}
	va_end(args);

	InsertMessageRaw(CStrW(szBuffer));
}
	

void CConsole::InsertMessageRaw(const CStrW& message)
{
	// (TODO: this text-wrapping is rubbish since we now use variable-width fonts)

	//Insert newlines to wraparound text where needed
	CStrW wrapAround(message);
	CStrW newline(L'\n');
	size_t oldNewline=0;
	size_t distance;
	
	//make sure everything has been initialized
	if ( m_charsPerPage != 0 )
	{
		while ( oldNewline+m_charsPerPage < wrapAround.length() )
		{
			distance = wrapAround.find(newline, oldNewline) - oldNewline;
			if ( distance > m_charsPerPage )
			{
				oldNewline += m_charsPerPage;
				wrapAround.insert( oldNewline++, newline );
			}
			else
				oldNewline += distance+1;
		}
	}
	// Split into lines and add each one individually
	oldNewline = 0;

	while ( (distance = wrapAround.find(newline, oldNewline)) != wrapAround.npos)
	{
		distance -= oldNewline;
		m_deqMsgHistory.push_front(wrapAround.substr(oldNewline, distance));
		oldNewline += distance+1;
	}
	m_deqMsgHistory.push_front(wrapAround.substr(oldNewline));
}

const wchar_t* CConsole::GetBuffer()
{
	m_szBuffer[m_iBufferLength] = 0;
	return( m_szBuffer );
}

void CConsole::SetBuffer(const wchar_t* szMessage, ...)
{
	int oldBufferPos = m_iBufferPos;	// remember since FlushBuffer will set it to 0

	va_list args;
	wchar_t szBuffer[CONSOLE_BUFFER_SIZE];

	va_start(args, szMessage);
		vswprintf(szBuffer, CONSOLE_BUFFER_SIZE, szMessage, args);
	va_end(args);

	FlushBuffer();

	wcsncpy(m_szBuffer, szMessage, CONSOLE_BUFFER_SIZE);
	m_iBufferLength = (int)wcslen(m_szBuffer);
	m_iBufferPos = std::min(oldBufferPos, m_iBufferLength);
}

void CConsole::UseHistoryFile( const CStr& filename, int max_history_lines )
{
	m_MaxHistoryLines = max_history_lines;

	m_sHistoryFile = filename;
	LoadHistory();
}

void CConsole::ProcessBuffer(const wchar_t* szLine){
	if (szLine == NULL) return;
	if (wcslen(szLine) <= 0) return;

	debug_assert(wcslen(szLine) < CONSOLE_BUFFER_SIZE);

	m_deqBufHistory.push_front(szLine);
	SaveHistory(); // Do this each line for the moment; if a script causes
	               // a crash it's a useful record.

	wchar_t szCommand[CONSOLE_BUFFER_SIZE];
	memset(szCommand, '\0', sizeof(wchar_t) * CONSOLE_BUFFER_SIZE);

	std::map<std::wstring, fptr>::iterator Iter;

	if (szLine[0] == '\\')
	{
		swscanf(szLine, L"\\%ls", szCommand);
		Trim(szCommand);
		ToLower(szCommand);

		if (!wcscmp(szCommand, L"info"))
		{
			InsertMessage(L"");
			InsertMessage(L"[Information]");
			InsertMessage(L"   -View commands \"\\commands\"");
			InsertMessage(L"   -Call command \"\\<command>\"");
			InsertMessage(L"   -Say \"<string>\"");
			InsertMessage(L"   -Help - Lists functions usable from console");
			InsertMessage(L"");
		}
		else if (!wcscmp(szCommand, L"commands"))
		{
			InsertMessage(L"");
			InsertMessage(L"[Commands]");

			if (!m_mapFuncList.size()) InsertMessage(L"   (none registered)");

			for (Iter = m_mapFuncList.begin(); Iter != m_mapFuncList.end(); Iter++)
				InsertMessage(L"   \\%ls", Iter->first.data());

			InsertMessage(L"");
		}
		else if (! (wcscmp(szCommand, L"Help") && wcscmp(szCommand, L"help")) )
		{
			InsertMessage(L"");
			InsertMessage(L"[Help]");
			InsertMessageRaw(m_helpText);
		}
		else
		{
			Iter = m_mapFuncList.find(szCommand);
			if (Iter == m_mapFuncList.end())
				InsertMessage(L"unknown command <%ls>", szCommand);
			else
				Iter->second();
		}
	}
	else if (szLine[0] == ':')
	{
		// Process it as JavaScript

		// (Cheating) run it as the first selected entity, if there is one.
		JSObject* RunAs = NULL;
		if( g_Selection.m_selected.size() )
			RunAs = g_Selection.m_selected[0]->GetScript();

		g_ScriptingHost.ExecuteScript( CStrW( szLine + 1 ), L"Console", RunAs );

	}
	else if (szLine[0] == '?')
	{
		// Process it as JavaScript and display the result

		// (Cheating) run it as the first selected entity, if there is one.
		JSObject* RunAs = NULL;
		if( g_Selection.m_selected.size() )
			RunAs = g_Selection.m_selected[0]->GetScript();

		jsval rval = g_ScriptingHost.ExecuteScript( CStrW( szLine + 1 ), L"Console", RunAs );
		if (rval)
		{
			try {
				InsertMessage( L"%ls", g_ScriptingHost.ValueToUCString( rval ).c_str() );
			} catch (PSERROR_Scripting_ConversionFailed) {
				InsertMessage( L"%hs", "<error converting return value to string>" );
			}
		}
	}
	else
		SendChatMessage(szLine);
}

void CConsole::LoadHistory()
{
	// note: we don't care if this file doesn't exist or can't be read;
	// just don't load anything in that case.

	// do this before vfs_load to avoid an error message if file not found.
	if(!vfs_exists(m_sHistoryFile))
		return;

	FileIOBuf buf; size_t buflen;
	if(vfs_load( m_sHistoryFile, buf, buflen ) < 0)
		return;

	CStr bytes( (char*)buf, buflen );
	(void)file_buf_free(buf);

	CStrW str( bytes.FromUTF8() );
	size_t pos = 0;
	while( pos != CStrW::npos )
	{
		pos = str.find( '\n' );
		if( pos != CStrW::npos )
		{
			if( pos > 0 )
				m_deqBufHistory.push_front( str.Left( str[pos-1] == '\r' ? pos - 1 : pos ) );
			str = str.GetSubstring( pos + 1, str.npos );
		}
		else if( str.Length() > 0 )
			m_deqBufHistory.push_front( str );
	}
}

void CConsole::SaveHistory()
{
	CStr buffer;
	std::deque<std::wstring>::iterator it;
	int line_count = 0;
	for( it = m_deqBufHistory.begin(); it != m_deqBufHistory.end(); ++it )
	{
		if(line_count++ >= m_MaxHistoryLines)
			break;
		buffer = CStrW( *it ).ToUTF8() + "\n" + buffer;
	}
	vfs_store( m_sHistoryFile, (const void*)buffer.c_str(), buffer.Length(), FILE_NO_AIO );
}

void CConsole::SendChatMessage(const wchar_t *szMessage)
{
	if (g_NetClient || g_NetServer)
	{
		CChatMessage *msg=new CChatMessage();
		msg->m_Recipient = PS_CHAT_RCP_ALL;
		msg->m_Message = szMessage;
		if (g_NetClient)
			g_NetClient->Push(msg);
		else
		{
			msg->m_Sender=g_NetServer->GetServerPlayerName();
			ReceivedChatMessage(msg->m_Sender.c_str(), msg->m_Message.c_str());
			g_NetServer->Broadcast(msg);
		}
	}
}

void CConsole::ReceivedChatMessage(const wchar_t *szSender, const wchar_t *szMessage)
{
	InsertMessage(L"%ls: %ls", szSender, szMessage);
}

extern CConsole* g_Console;

extern void Die(int err, const wchar_t* fmt, ...);

static bool isUnprintableChar(SDL_keysym key)
{
	// U+0000 to U+001F are control characters
	if (key.unicode < 0x20)
	{
		switch (key.sym)
		{
			// We want to allow some, which are handled specially
		case SDLK_RETURN: case SDLK_TAB:
		case SDLK_BACKSPACE: case SDLK_DELETE:
		case SDLK_HOME: case SDLK_END:
		case SDLK_LEFT: case SDLK_RIGHT:
		case SDLK_UP: case SDLK_DOWN:
		case SDLK_PAGEUP: case SDLK_PAGEDOWN:
			return false;

			// Ignore the others
		default:
			return true;
		}
	}

	return false;
}

InReaction conInputHandler(const SDL_Event_* ev)
{
	if( ev->ev.type == SDL_HOTKEYDOWN )
	{
		if( ev->ev.user.code == HOTKEY_CONSOLE_TOGGLE )
		{
			g_Console->ToggleVisible();
			return IN_HANDLED;
		}
		else if( ev->ev.user.code == HOTKEY_CONSOLE_COPY )
		{
			sys_clipboard_set( g_Console->GetBuffer() );
			return IN_HANDLED;
		}
		else if( ev->ev.user.code == HOTKEY_CONSOLE_PASTE )
		{
			wchar_t* text = sys_clipboard_get();
			if(text)
			{
				for(wchar_t* c = text; *c; c++)
					g_Console->InsertChar(0, *c);

				sys_clipboard_free(text);
			}
			return IN_HANDLED;
		}
	}

	if( ev->ev.type != SDL_KEYDOWN)
		return IN_PASS;

	SDLKey sym = ev->ev.key.keysym.sym;

	if(!g_Console->IsActive())
		return IN_PASS;

	// Stop unprintable characters (ctrl+, alt+ and escape),
	// also prevent ` and/or ~ appearing in console every time it's toggled.
	if( !isUnprintableChar(ev->ev.key.keysym) &&
		!hotkeys[HOTKEY_CONSOLE_TOGGLE] )
		g_Console->InsertChar(sym, (wchar_t)ev->ev.key.keysym.unicode );

	return IN_PASS;
}
