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

iOk     ="确认";
iCancel ="取消";
iDone   ="结束";

iGoBack    ="前页";
iGoForward    ="后页";
iReload      ="重载";
iStop      ="停止";
iStopped      ="停止";
iHistory    ="历史";
iShowBookmarks   ="显示书签";
iEditBookmarks   ="编辑书签";
iConfig   ="配置";
iHomePage  ="主页";
iHelp    ="帮助";
iCopyLoc  ="Copy location" ;
iSoftKeyboard  ="Software keyboard";
iExit    ="退出";
iUserPreferences   ="用户设置";
iProxyCaption1   ="代理用来通过防火墙";
iProxyCaption2   ="访问 Internet";
iProxyLabel     ="代理服务 : ";
iPortLabel      ="端口 : ";
iNoproxyLabel   ="不用代理 : ";
iHomepageLabel  ="主页: ";
iLoadimageLabel   ="自动下载图像 : ";
iMoveupButton     ="向上移";
iMovedownButton     ="向下移";
iDeleteButton     ="删除";
iAddButton     ="增加";
iTitleLabel    ="标题 : ";
iUrlLabel   ="URL : ";
iFetchStatus   ="取 ... ";
iContinue  ="继续";
iSwProtocol  ="切换协议";
iCreated  ="已建立";
iAccepted  ="已接收";
iNonAuth  ="无授权";
iNoContent  ="无内容";
iResetContent  ="重置内容";
iPartContent  ="部分内容";
iMultiChoice   ="多选择";
iMovedPerm  ="永久被移";
iMovedTemp  ="暂时被移";
iSeeOther   ="看其它";
iNotModify   ="未改动";
iUseProxy   ="使用代理";
iBadReq   ="无效请求";
iUnauth   ="未授权";
iPayRequired    ="需要付费";
iForbidden    ="禁止";
iNotFound     ="未找到";
iNotAllowed    ="不允许";
iNotAccpt  ="不接收";
iProxyAuth   ="需要代理授权";
iReqTimeout   ="请求超时";
iConflict    ="冲突";
iGone   ="Gone";
iLenRequired   ="需要长度";
iPrecondFailed   ="先决条件失败";
iReqTooLarge   ="请求包太大";
iUriTooLarge   ="请求URI太大 ";
iUnsuppMedia     ="不支持的媒体类型";
iRangeInvalid    ="请求范围失效";
iExpectFailed   ="期望失败";
iServerError    ="服务器错务";
iNotImplement    ="未实现";
iBadGateway     ="无效 Gateway";
iServUnavail    ="无效服务";
iGatewayTimeout   ="Gateway 超时";
iVerUnsupp     ="不支持的HTTP 版本";
iRedirFailed    ="重定向失败";
iUnknownCode      ="未知代码";
iRedirEmail  ="重定向电子邮件";
iFailMail    ="电子邮件失败";
iEmptyPage   ="空页";
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
