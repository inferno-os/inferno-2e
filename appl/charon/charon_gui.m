####
#
# Charon_gui.m
# by : watson zhou
# Thu Jan 28 14:04:49 CST 1999 
#######################
Charon_gui : module
{ 
PATH : con "/dis/charon/charon_gui.dis";
 
init: fn();

CtlFnt: int;
Fontinfo: adt{
	name: string;
	f: ref Draw->Font;
	spw: int;
};

fonts: array of Fontinfo;

iOk	:string;
iCancel	:string;
iDone	:string;
iGoBack     :string;    	#back to previous page
iGoForward     :string;    #forward to next page 
iReload       :string;  
iStop       :string;     #stop loading
iStopped       :string;     #loading stopped
iHistory     :string;  
iShowBookmarks    :string;  
iEditBookmarks    :string;  
iConfig    :string;   
iHomePage   :string;  
iHelp     :string;  
iCopyLoc  :string;
iSoftKeyboard  :string;
iExit     :string;  
iUserPreferences    :string;  #explain message 
iProxyCaption1      :string; #explain message 
iProxyCaption2      :string;  #explain message 
iProxyLabel      :string;  
iPortLabel       :string;  
iNoproxyLabel    :string;  
iHomepageLabel   :string;  
iLoadimageLabel    :string;     #check box:if load or not
iMoveupButton      :string;    #not used 
iMovedownButton      :string;    #not used 
iDeleteButton      :string;  #not used 
iAddButton      :string;  #not used 
iTitleLabel     :string;  
iUrlLabel    :string;  
iFetchStatus    :string;  
iContinue   :string;    #returned from proxy 
iSwProtocol   :string;    #returned from proxy 
iCreated   :string;    #returned from proxy 
iAccepted   :string;    #returned from proxy 
iNonAuth   :string;    #returned from proxy 
iNoContent   :string;    #returned from proxy 
iResetContent   :string;    #returned from proxy 
iPartContent   :string;    #returned from proxy 
iMultiChoice    :string;    #returned from proxy 
iMovedPerm   :string;    #returned from proxy 
iMovedTemp   :string;    #returned from proxy 
iSeeOther    :string;    #returned from proxy 
iNotModify    :string;    #returned from proxy 
iUseProxy    :string;    #returned from proxy 
iBadReq    :string;    #returned from proxy 
iUnauth    :string;    #returned from proxy 
iPayRequired     :string;    #returned from proxy 
iForbidden     :string;    #returned from proxy 
iNotFound      :string;    #returned from proxy 
iNotAllowed     :string;    #returned from proxy 
iNotAccpt   :string;    #returned from proxy 
iProxyAuth    :string;    #returned from proxy 
iReqTimeout    :string;    #returned from proxy 
iConflict     :string;    #returned from proxy 
iGone    :string;    #returned from proxy 
iLenRequired    :string;    #returned from proxy 
iPrecondFailed    :string;    #returned from proxy 
iReqTooLarge    :string;    #returned from proxy 
iUriTooLarge    :string;    #returned from proxy 
iUnsuppMedia      :string;    #returned from proxy 
iRangeInvalid     :string;    #returned from proxy 
iExpectFailed    :string;    #returned from proxy 
iServerError     :string;    #returned from proxy 
iNotImplement     :string;    #returned from proxy 
iBadGateway      :string;    #returned from proxy 
iServUnavail     :string;    #returned from proxy 
iGatewayTimeout    :string;    #returned from proxy 
iVerUnsupp      :string;    #returned from proxy 
iRedirFailed     :string;    #returned from proxy 
iUnknownCode       :string;    #returned from proxy 
iRedirEmail   :string;    #returned from proxy 
iFailMail     :string;    #returned from proxy 
iEmptyPage    :string;    #returned from proxy 
iSaving       :string;     
iLogoBit     :string;    #charon logo 
iBackBit     :string;    #back to previous page button image
iFwdBit     :string;    #forward to next page button image
iReloadBit     :string;   #reload button image
iStopBit     :string;      
iHistoryBit     :string;  
iBookmarkBit     :string;    #show bookmark button image
iEditBit     :string;      #edit bookmark button image
iConfBit     :string;      #configuration button image
iHelpBit     :string;      #help button image
iHomeBit     :string;      #go to homepage button image
iSslonBit     :string;    #not used
iSsloffBit     :string;   #not used
iUpBit     :string;       #in edit bookmark
iDownBit     :string;       #in edit bookmark
iPlusBit     :string;       #in edit bookmark
iMinusBit     :string;       #in edit bookmark
iExitBit     :string;       #in edit bookmark
iCopyBit      :string;     #copy  image
iKbdBit               :string;        #keyboad image
iCharonLabel   :string;
iUseSSL		:string; #use ssl
iSSLV2		:string; #use ssl version 2
iSSLV3		:string; #use ssl version 3
};
