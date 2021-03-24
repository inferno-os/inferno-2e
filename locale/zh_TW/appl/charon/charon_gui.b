####
# 
# Charon_gui.b
# by : watson zhou
#Tue Mar  9 17:36:05 CST 1999 
######################
implement  Charon_gui ;

include "common.m";
include "charon_gui.m";

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
CtlFnt=2;
fonts=array[NumFnt] of {Fontinfo
        ("/fonts/lucidasans/unicode.6.font", nil, 0),
        ("/fonts/lucidasans/unicode.7.font", nil, 0),
        #("/fonts/lucidasans/unicode.8.font", nil, 0),
        ("/fonts/lucm/unicode.9.font", nil, 0),
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

iOk     ="Ok";
iCancel ="Cancel";
iDone   ="結束";

iGoBack    ="前頁";
iGoForward    ="後頁";
iReload      ="重載";
iStop      ="停止";
iStopped      ="停止";
iHistory    ="歷史";
iShowBookmarks   ="顯示書簽";
iEditBookmarks   ="編輯書簽";
iConfig   ="配置";
iHomePage  ="主頁";
iHelp    ="幫助";
iCopyLoc  ="Copy location" ;
iSoftKeyboard  ="Software keyboard";
iExit    ="退出";
iUserPreferences   ="用戶";
iProxyCaption1   ="網絡代理用來訪問 internet";
iProxyCaption2   ="通過防火牆";
iProxyLabel     ="網絡代理 : ";
iPortLabel      ="端口 : ";
iNoproxyLabel   ="無防火牆 : ";
iHomepageLabel  ="主頁: ";
iLoadimageLabel   ="自動下載圖像 : ";
iMoveupButton     ="向上移";
iMovedownButton     ="向下移";
iDeleteButton     ="刪除";
iAddButton     ="增加";
iTitleLabel    ="標題 : ";
iUrlLabel   ="URL : ";
iFetchStatus   ="取 ... ";
iContinue  ="繼續";
iSwProtocol  ="切換協議";
iCreated  ="已建立";
iAccepted  ="已接收";
iNonAuth  ="無授權";
iNoContent  ="無内容";
iResetContent  ="重置内容";
iPartContent  ="部分内容";
iMultiChoice   ="多選擇";
iMovedPerm  ="永久被移";
iMovedTemp  ="暫時被移";
iSeeOther   ="看其它";
iNotModify   ="未改過";
iUseProxy   ="使用代理";
iBadReq   ="無效請求";
iUnauth   ="未授權";
iPayRequired    ="需要負費";
iForbidden    ="禁止";
iNotFound     ="未找到";
iNotAllowed    ="不允許";
iNotAccpt  ="不接收";
iProxyAuth   ="需要代理授權";
iReqTimeout   ="請求超時";
iConflict    ="沖突";
iGone   ="Gone";
iLenRequired   ="需要長度";
iPrecondFailed   ="先決條件失敗";
iReqTooLarge   ="請求包太長";
iUriTooLarge   ="請求URI太長 ";
iUnsuppMedia     ="不支持的媒體類型";
iRangeInvalid    ="請求范圍無效";
iExpectFailed   ="假設失敗";
iServerError    ="内部服務器失敗";
iNotImplement    ="未實現";
iBadGateway     ="無效 Gateway";
iServUnavail    ="無效服務";
iGatewayTimeout   ="Gateway 超時";
iVerUnsupp     ="不支持的HTTP 版本";
iRedirFailed    ="轉向失敗";
iUnknownCode      ="未知代碼";
iRedirEmail  ="轉向電子郵件";
iFailMail    ="載入電子郵件失敗";
iEmptyPage   ="空頁";
iSaving      ="保存";
iLogoBit    ="charon.bit";
iBackBit    ="redleft.bit";
iFwdBit    ="redright.bit";
iReloadBit    ="circarrow.bit";
iStopBit    ="stop.bit";
iHistoryBit    ="history.bit";
iBookmarkBit    ="bookmark.bit";
iEditBit    ="edit.bit";
iConfBit    ="conf.bit";
iHelpBit    ="help.bit";
iHomeBit    ="home.bit";
iSslonBit    ="sslon.bit";
iSsloffBit    ="ssloff.bit";
iUpBit    ="up.bit";
iDownBit    ="down.bit";
iPlusBit    ="plus.bit";
iMinusBit    ="minus.bit";
iExitBit    ="exit.bit";
iCopyBit      ="copy.bit";
iKbdBit               ="keyboard.bit";
iCharonLabel  ="Charon";
iUseSSL = "Use SSL: ";
iSSLV2 = "V2 ";
iSSLV3 = "V3 ";
}
