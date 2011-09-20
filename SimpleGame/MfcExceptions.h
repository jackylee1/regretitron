#pragma once
#include <exception>

// This provides macros to use to wrap your code
// it converts std::exceptions to CExceptions.
// http://members.cox.net/doug_web/eh.htm
// Apparently, MFC can't handle non CException objects getting thrown through it... mine were disappearing.
// MFC handles well uncaught CException's. (Displays a nice error box with the GetErrorMessage message)

// Wrap your code in MFC_STD_EH_PROLOGUE and MFC_STD_EH_EPILOGUE, defined below.

class MfcGenericException : public CException
{
public:

   // CException overrides
   BOOL GetErrorMessage(
         LPTSTR lpszError,
         UINT nMaxError,
         PUINT pnHelpContext = 0)
   {
      ASSERT(lpszError != 0);
      ASSERT(nMaxError != 0);
      if (pnHelpContext != 0)
         *pnHelpContext = 0;
      _tcsncpy(lpszError, m_msg, nMaxError-1);
      lpszError[nMaxError-1] = 0;
      return *lpszError != 0;
   }

protected:

   explicit MfcGenericException(const CString& msg)
   :  m_msg(msg)
   {
   }

private:

   CString m_msg;
};

class MfcStdException : public MfcGenericException
{
public:

   static MfcStdException* Create(const std::exception& ex)
   {
      return new MfcStdException(ex);
   }

private:

   explicit MfcStdException(const std::exception& ex)
   : MfcGenericException(ex.what())
   {
   }
};

#define MFC_STD_EH_PROLOGUE try {
#define MFC_STD_EH_EPILOGUE \
      } catch (std::exception& ex) { throw MfcStdException::Create(ex); }
