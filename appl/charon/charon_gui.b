####
# 
# Charon_gui.b
# by : watson zhou
#Thu Jan 28 14:04:49 CST 1999 
######################
implement Charon_gui;

include "common.m";
include "charon_gui.m";
#include "shannon_gui.m";
#	shannon_gui: Shannon_GUI;

sys:Sys;
CU: CharonUtils;
	ByteSource,CImage: import CU;
D: Draw;
	Font,Point, Rect, Image: import D;
U: Url;
	ParsedUrl: import U;
B: Build;
	NumFnt: import B;

init()
{
	#shannon_gui=load Shannon_GUI Shannon_GUI->PATH;
	#if(shannon_gui==nil)
	#	CU->raise(sys->sprint("Exinternal: couldn't load Shannon_GUI:%r"));
	#shannon_gui->init();

	CtlFnt=1;
	fonts=array[NumFnt] of {Fontinfo
	("/fonts/lucidasans/unicode.6.font", nil, 0),
        ("/fonts/lucidasans/unicode.7.font", nil, 0),
        ("/fonts/lucidasans/unicode.8.font", nil, 0),
        #("/fonts/lucm/unicode.9.font", nil, 0),
        ("/fonts/lucidasans/unicode.10.font", nil, 0),
        ("/fonts/lucidasans/unicode.13.font", nil, 0),
        ("/fonts/lucidasans/italicunicode.6.font", nil, 0),
        ("/fonts/lucidasans/italicunicode.7.font", nil, 0),
        ("/fonts/lucidasans/italicunicode.8.font", nil, 0),
        ("/fonts/lucidasans/italicunicode.10.font", nil, 0),
        ("/fonts/lucidasans/italicunicode.13.font", nil, 0),
        ("/fonts/lucidasans/boldunicode.6.font", nil, 0),
        ("/fonts/lucidasans/boldunicode.7.font", nil, 0),
        ("/fonts/lucidasans/boldunicode.8.font", nil, 0),
        ("/fonts/lucidasans/boldunicode.10.font", nil, 0),
        ("/fonts/lucidasans/boldunicode.13.font", nil, 0),
        ("/fonts/lucidasans/typeunicode.6.font", nil, 0),
        ("/fonts/lucidasans/typeunicode.7.font", nil, 0),
        ("/fonts/lucidasans/typeunicode.9.font", nil, 0),
        ("/fonts/lucidasans/typeunicode.12.font", nil, 0),
        ("/fonts/lucidasans/typeunicode.16.font", nil, 0)
};

iOk	="Ok";
iCancel	="Cancel";
iDone	="Done";

iGoBack     ="Go back";
iGoForward     ="Go forward";
iReload       ="Reload current page";
iStop       ="Stop";
iStopped       ="Stopped";
iHistory     ="Show history";
iShowBookmarks    ="Show bookmarks";
iEditBookmarks    ="Edit bookmarks";
iConfig    ="Config";
iHomePage   ="Home page";
iHelp     ="Help";
iCopyLoc  ="Copy location" ;
iSoftKeyboard  ="Software keyboard";
iExit     ="Exit Charon";
iUserPreferences    ="User Preferences";
iProxyCaption1      ="The network proxy is used to access the internet ";
iProxyCaption2      ="through a firewall ";
iProxyLabel      ="Proxy : ";
iPortLabel       ="Port : ";
iNoproxyLabel    ="No Proxy For : ";
iHomepageLabel   ="Homepage: ";
iLoadimageLabel    ="Automatic Load Images : ";
iMoveupButton      ="MoveUp";
iMovedownButton      ="MoveDown";
iDeleteButton      ="Delete";
iAddButton      ="Add";
iTitleLabel     ="Title : ";
iUrlLabel    ="URL : ";
iFetchStatus    ="Fetching ";
iContinue   ="Continue";
iSwProtocol   ="Switching Protocols";
iCreated   ="Created";
iAccepted   ="Accepted";
iNonAuth   ="Non-Authoratative Information";
iNoContent   ="No content";
iResetContent   ="Reset content";
iPartContent   ="Partial content";
iMultiChoice    ="Multiple choices";
iMovedPerm   ="Moved permanently";
iMovedTemp   ="Moved temporarily";
iSeeOther    ="See other";
iNotModify    ="Not modified";
iUseProxy    ="Use proxy";
iBadReq    ="Bad request";
iUnauth    ="Unauthorized";
iPayRequired     ="Payment required";
iForbidden     ="Forbidden";
iNotFound      ="Not found";
iNotAllowed     ="Method not allowed";
iNotAccpt   ="Not Acceptable";
iProxyAuth    ="Proxy authentication required";
iReqTimeout    ="Request timed-out";
iConflict     ="Conflict";
iGone    ="Gone";
iLenRequired    ="Length required";
iPrecondFailed    ="Precondition failed";
iReqTooLarge    ="Request entity too large";
iUriTooLarge    ="Request-URI too large";
iUnsuppMedia      ="Unsupported media type";
iRangeInvalid     ="Requested range not valid";
iExpectFailed    ="Expectation failed";
iServerError     ="Internal server error";
iNotImplement     ="Not implemented";
iBadGateway      ="Bad gateway";
iServUnavail     ="Service unavailable";
iGatewayTimeout    ="Gateway time-out";
iVerUnsupp      ="HTTP version not supported";
iRedirFailed     ="Redirection failed";
iUnknownCode       ="Unknown code";
iRedirEmail   ="Redirect Email ";
iFailMail     ="Failed to load mailtool\n";
iEmptyPage    ="Empty page";
iSaving       ="Saving";
iLogoBit     ="charon.bit";
iBackBit     ="redleft.bit";
iFwdBit     ="redright.bit";
iReloadBit     ="circarrow.bit";
iStopBit     ="stop.bit";
iHistoryBit     ="history.bit";
iBookmarkBit     ="bookmark.bit";
iEditBit     ="edit.bit";
iConfBit     ="conf.bit";
iHelpBit     ="help.bit";
iHomeBit     ="home.bit";
iSslonBit     ="sslon.bit";
iSsloffBit     ="ssloff.bit";
iUpBit     ="up.bit";
iDownBit     ="down.bit";
iPlusBit     ="plus.bit";
iMinusBit     ="minus.bit";
iExitBit     ="exit.bit";
iCopyBit      ="copy.bit";
iKbdBit               ="keyboard.bit";
iCharonLabel   ="Charon";
iUseSSL	= "Use SSL: ";
iSSLV2 = "V2 ";
iSSLV3 = "V3 ";
}
