/************************************************************************
| token.C
| Written by: Morcallen
| Date: 27 July 1996
| Description:  This file contains the class declaration and implementation
|   for the Token and TokenList classes, which are used by the act()
|   function to split up the input stream.
*/
// Standard header files

extern "C"
{
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
}
// DarkCastle header files
#include "db.h"
#include "room.h"
#include "character.h" // char_data
#include "obj.h"       // obj_data
#include "levels.h"    // MIN_GOD
#include "utility.h"   // GET_SHORT, GET_LEVEL, &c
#include "terminal.h"  // colors
#include "act.h"       // act flags
#include "player.h"    // PLR_ANSI PLR_VT100
#include "handler.h"   // fname()
#include "token.h"     // fname()
#include "connect.h"

#undef DEBUG_TOKEN

extern CWorld world;

/************************************************************************
| TokenList::TokenList(char *str)
| Preconditions: str != 0
| Postconditions: TokenList is ready to go: the tokens have been assigned,
|   the types set, &c..
| Returns: Nothing
*/
TokenList::TokenList(const char *str) : head(0), current(0)
{

  int i, stri = 0;
  const char *strp = str;  // Keeps track of current position
  Token *cur_token;        // Current token
  char temp_str[_MAX_STR]; // Temporary string

  while (strp[stri] != 0 && stri < _MAX_STR)
  {
    if (strp[stri] == '$') /* It's a string code */
    {
      temp_str[0] = strp[stri + 0];
      temp_str[1] = strp[stri + 1];
      temp_str[2] = 0;
      stri += 2;
    }
    else /* Go until we find a string code or end */
    {
      for (i = 0; (strp[stri] != 0) && (strp[stri] != '$') && (stri < _MAX_STR); i++, stri++)
      {
        temp_str[i] = strp[stri];
      }
      temp_str[i] = 0;
    }

    cur_token = new Token(temp_str);
    AddToken(cur_token);

#ifdef DEBUG_TOKEN
    cerr << "Added token: " << cur_token->GetBuf() << endl;
#endif
  }

  Reset();
}

/************************************************************************
| TokenList::Reset()
| Description:  Sets current = head, and quits.
*/
void TokenList::Reset()
{
  current = head;
}

/************************************************************************
| TokenList::~TokenList()
| Description:  Destructor.  Removes all Tokens and frees all memory.
| Returns: Nothing
*/
TokenList::~TokenList()
{
  Token *temp_token, *next_token;

  Reset();

  for (temp_token = current; temp_token; temp_token = next_token)
  {
    next_token = temp_token->Next();
    delete temp_token;
  }
}

/************************************************************************
| TokenList::AddToken(Token *new_token)
| Description:  We trust this is only called by the constructor, so
|   we don't worry about it too much.  The new token is inserted after
|   whatever current is, unless current == 0, in which case, it becomes
|   current.
*/
void TokenList::AddToken(const Token *new_token)
{
  if (current == 0)
  {
    current = (Token *)new_token;
    head = current;
  }
  else
  {
    current->Next((Token *)new_token);
    current = current->Next();
  }
}

/************************************************************************
| void TokenList::Next()
| Description:  Moves to the next token
*/
void TokenList::Next()
{
  current = current->Next();
}

/************************************************************************
| const char * TokenList::Interpret()
| Description:  This function interprets the tokens in the list for
|   the given character/victim/send_to combination and then returns
|   the interpreted string.  This string should not be deallocated.
|   Zero is returned (0) if the string should not be sent -- either
|   the send_to is asleep or the INVIS_NULL flag was used (for example).
*/
string TokenList::Interpret(char_data *from, obj_data *obj, void *vict_obj, char_data *send_to, int flags)
{

  // Reset the string
  interp.clear();

  //--
  // Can't write to a mob!
  //--
  // However, we want to go ahead and format what they would get sent so that the
  // MOBProgs will trigger.  Added check to send_to->desc on next line as well since
  // we no longer check it here.
  // - pir 11/18/01
  // if(send_to->desc == 0) return(0);

  //--
  // A few checks before we bother
  //--
  // If they're not playing and force flag isn't set, don't send
  if (send_to == nullptr)
  {
    return "";
  }
  if (send_to->desc && send_to->desc->connected != conn::PLAYING && !(flags & FORCE))
    return "";
  if (IS_SET(world[send_to->in_room].room_flags, QUIET) && !(flags & FORCE))
    return "";
  if ((send_to == (char_data *)vict_obj) && (flags & NOTVICT))
    return "";
  if ((GET_LEVEL(send_to) < MIN_GOD) && (flags & GODS))
    return "";
  if ((GET_POS(send_to) <= POSITION_SLEEPING) && !(flags & ASLEEP))
    return "";

  // Ok, now bother
  for (Reset(); current != 0; Next())
  {

#ifdef DEBUG_TOKEN
    if (current->IsAnsi())
      cerr << "ANSI token" << endl;
    if (current->IsVt100())
      cerr << "VT100 token" << endl;
    if (current->IsCode())
      cerr << "Code token" << endl;
    if (current->IsText())
      cerr << "Text token" << endl;
#endif

    if (current->IsText())
    {
#ifdef DEBUG_TOKEN
      cerr << "It's a text token" << endl;
#endif
      interp += current->GetBuf();
    }
    else if (current->IsAnsi() || current->IsVt100())
    {
#ifdef DEBUG_TOKEN
      cerr << "It's ansi or vt100 code" << endl;
#endif
      if (IS_MOB(send_to) ||
          (IS_SET(send_to->pcdata->toggles, PLR_ANSI) && current->IsAnsi()) ||
          (IS_SET(send_to->pcdata->toggles, PLR_VT100) && current->IsVt100()))
      {
        switch (current->GetBuf()[1])
        {
        case '1':
          interp += BLUE;
          break;
        case '2':
          interp += GREEN;
          break;
        case '3':
          interp += CYAN;
          break;
        case '4':
          interp += RED;
          break;
        case '5':
          interp += YELLOW;
          break;
        case '6':
          interp += PURPLE;
          break;
        case '7':
          interp += GREY;
          break;
        case '0':
          interp += BLACK;
          break;
        case 'B':
          interp += BOLD;
          break;
        case 'L':
          interp += FLASH;
          break;
        case 'I':
          interp += INVERSE;
          break;
        case 'R':
          interp += NTEXT;
          break;
        case '*':
          interp += EEEE;
          break;
        case '$':
          interp += "$";
          break;
        default:
          break;
        } // switch
      }   // if they are appropriate
    }     // if it's ansi or vt100
    else if (current->IsCode())
    {
#ifdef DEBUG_TOKEN
      cerr << "It's a special code" << endl;
#endif
      switch ((current->GetBuf())[1])
      {
      case 'n':
        if (send_to == nullptr || from == nullptr || GET_SHORT(from) == nullptr)
        {
          break;
        }

        if (!CAN_SEE(send_to, from, true))
        {
          if (flags & INVIS_NULL)
            return {};
          else if (flags & INVIS_VISIBLE)
            interp += GET_SHORT(from);
          else
            interp += "someone";
        }
        else
        {
          if (GET_SHORT(from))
          {
            interp += GET_SHORT(from);
          }
        }
        break;
      case 'N':
        if (vict_obj == nullptr || GET_SHORT((char_data *)vict_obj) == nullptr)
        {
          break;
        }
        if (!CAN_SEE(send_to, (char_data *)vict_obj, true))
        {
          if (flags & INVIS_NULL)
            return {};
          else if (flags & INVIS_VISIBLE)
            interp += GET_SHORT((char_data *)vict_obj);
          else
            interp += "someone";
        }
        else
        {
          if (vict_obj == nullptr || GET_SHORT((char_data *)vict_obj) == nullptr)
          {
            break;
          }
          interp += GET_SHORT((char_data *)vict_obj);
        }
        break;
      case 'm':
        if (from == nullptr)
        {
          break;
        }
        interp += HMHR(from);
        break;
      case 'M':
        if (vict_obj == nullptr)
        {
          break;
        }
        interp += HMHR((char_data *)vict_obj);
        break;
      case 's':
        if (from == nullptr)
        {
          break;
        }
        interp += HSHR(from);
        break;
      case 'S':
        if (vict_obj == nullptr)
        {
          break;
        }
        interp += HSHR((char_data *)vict_obj);
        break;
      case 'e':
        if (from == nullptr)
        {
          break;
        }
        interp += HSSH(from);
        break;
      case 'E':
        if (vict_obj == nullptr)
        {
          break;
        }
        interp += HSSH((char_data *)vict_obj);
        break;
      case 'o':
        if (send_to == nullptr || obj == nullptr || obj->name == nullptr)
        {
          break;
        }

        if (!CAN_SEE_OBJ(send_to, obj))
        {
          if (flags & INVIS_NULL)
            return {};
          else if (flags & INVIS_VISIBLE)
          {
            interp += fname(obj->name);
          }
          else
            interp += "something";
        }
        else
        {
          interp += fname(obj->name);
        }
        break;
      case 'O':
        if (send_to == nullptr || vict_obj == nullptr || ((obj_data *)vict_obj)->name == nullptr)
        {
          break;
        }
        if (!CAN_SEE_OBJ(send_to, (obj_data *)vict_obj))
        {
          if (flags & INVIS_NULL)
            return {};
          else if (flags & INVIS_VISIBLE)
            interp += fname(((obj_data *)vict_obj)->name);
          else
            interp += "something";
        }
        else
        {
          interp += fname(((obj_data *)vict_obj)->name);
        }
        break;
      case 'p':
        if (send_to == nullptr || obj == nullptr || obj->short_description == nullptr)
        {
          break;
        }

        if (!CAN_SEE_OBJ(send_to, obj))
        {
          if (flags & INVIS_NULL)
            return {};
          else if (flags & INVIS_VISIBLE)
            interp += obj->short_description;
          else
            interp += "something";
        }
        else
        {
          interp += obj->short_description;
        }
        break;
      case 'P':
        if (send_to == nullptr || vict_obj == nullptr || ((obj_data *)vict_obj)->short_description == nullptr)
        {
          break;
        }

        if (!CAN_SEE_OBJ(send_to, (obj_data *)vict_obj))
        {
          if (flags & INVIS_NULL)
            return {};
          else if (flags & INVIS_VISIBLE)
            interp += ((obj_data *)vict_obj)->short_description;
          else
            interp += "something";
        }
        else
        {
          interp += ((obj_data *)vict_obj)->short_description;
        }
        break;
      case 'a':
        if (obj == nullptr || obj->name == nullptr)
        {
          break;
        }

        switch (*(obj)->name)
        {
        case 'a':
        case 'A':
        case 'e':
        case 'E':
        case 'i':
        case 'I':
        case 'o':
        case 'O':
        case 'u':
        case 'U':
        case 'y':
        case 'Y':
          interp += "an";
          break;
        default:
          interp += "a";
          break;
        }
        break;
      case 'A':
        if (vict_obj != nullptr && ((obj_data *)vict_obj)->name != nullptr)
        {
          switch (*((obj_data *)vict_obj)->name)
          {
          case 'a':
          case 'A':
          case 'e':
          case 'E':
          case 'i':
          case 'I':
          case 'o':
          case 'O':
          case 'u':
          case 'U':
          case 'y':
          case 'Y':
            interp += "an";
            /* no break */
          default:
            interp += "a";
          }
        }
        break;
      case 'T':
        if (vict_obj != nullptr)
        {
          interp += (char *)vict_obj;
        }
        break;

      case 'F':
        if (vict_obj != nullptr)
        {
          interp += fname((char *)vict_obj);
        }
        break;

      default: // Illegal code - just output it
        interp += current->GetBuf();
        break;
      }  /* switch */
    }    /* if it's a code */
    else // It's unrecognized.  Shouldn't happen.
    {
      log("TokenList::Interpret() sent bad Token!", OVERSEER, LOG_BUG);
    }
#ifdef DEBUG_TOKEN
    cerr << "Output after this loop: " << interp << endl;
#endif
  } /* for loop */

  interp += "\r\n";

#ifdef DEBUG_TOKEN
  cerr << "Finished building interp; it is:" << endl;
  cerr << interp << endl;
#endif

  return interp;
}

/************************************************************************
| TokenList::Token::Token(char *tok_str)
| Description:  Used to create a new token
*/
Token::Token(char *tok_str) : buf(0), type(0), next(0)
{
  SetBuf(tok_str);
}

/************************************************************************
| TokenList::Token::~Token()
| Description: Destructor -- called whenever a Token goes out of scope
|   or is destroyed.
*/
Token::~Token()
{
  if (buf)
    delete[] buf;
  buf = 0;
}

/************************************************************************
| TokenList::Token::SetBuf(char *rhs)
| Description: Used to set the buffer
*/
void Token::SetBuf(char *rhs)
{
  if (buf)
    delete[] buf;
  buf = new char[strlen(rhs) + 1];

  strcpy(buf, rhs);

  if (buf[0] != '$')
  {
    type = TEXT;
#ifdef DEBUG_TOKEN
    cerr << buf << ": TEXT" << endl;
#endif
  }
  else
  {
    //--
    // This switch statement just assigns the type of token we're dealing
    // with.  If you add new colors &c, you should modify this.
    // $$ is a little tricky, it truncates the string so that only one $
    // appears on the end result output
    // $$ now just prints both since it's handled in 'handle_ansi' -pir 2/14/01
    //--
    switch (buf[1])
    {
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '0':
    case '*':
      type = ANSI;
#ifdef DEBUG_TOKEN
      cerr << buf << ": ANSI" << endl;
#endif
      break;
    case 'B':
    case 'I':
    case 'L':
    case 'R':
      type = (VT100 | ANSI);
#ifdef DEBUG_TOKEN
      cerr << buf << ": ANSI|VT100" << endl;
#endif
      break;
      // we allow $$ to go through now, since it's handled in handle_ansi -pir 2/14/01
    case '$':
      type = TEXT; // buf[1] = 0;
#ifdef DEBUG_TOKEN
      cerr << buf << ": TEXT" << endl;
#endif
      break;
    default:
      type = CODE;
#ifdef DEBUG_TOKEN
      cerr << buf << ": CODE" << endl;
#endif
      break;
    } /* switch */
  }   /* else */
}
