// This file is part of the aMule Project
//
// Copyright (c) 2003-2004 aMule Project ( http://www.amule-project.net )
// Copyright (C) 2002 Merkur ( merkur-@users.sourceforge.net / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "types.h"

#ifdef __WXMSW__
	#include <winsock.h>
#else
#ifdef __OPENBSD__
       #include <sys/types.h>
#endif /* __OPENBSD__ */
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
#endif

#include <zlib.h>		// Needed for inflateEnd
#include <wx/defs.h>		// Needed before any other wx/*.h
#include <wx/object.h>		// Needed by wx/sckaddr.h
#include <wx/sckaddr.h>		// Needed for wxIPV4address
#include "otherfunctions.h"	// Needed for nstrdup

#include "SearchList.h"		// Needed for CSearchList
#include "ChatWnd.h"		// Needed for CChatWnd
#include "DownloadQueue.h"	// Needed for CDownloadQueue
#include "DownloadListCtrl.h"	// Needed for CDownloadListCtrl
#include "TransferWnd.h"	// Needed for CTransferWnd
#include "UploadQueue.h"	// Needed for CUploadQueue
#include "IPFilter.h"		// Needed for CIPFilter
#include "sockets.h"		// Needed for CServerConnect
#include "ClientCredits.h"	// Needed for CClientCreditsList
#include "ServerListCtrl.h"	// Needed for CServerListCtrl
#include "ServerWnd.h"		// Needed for CServerWnd
#include "amuleDlg.h"		// Needed for CamuleDlg
#include "server.h"		// Needed for CServer
#include "Preferences.h"	// Needed for CPreferences
#include "SafeFile.h"		// Needed for CSafeMemFile
#include "packets.h"		// Needed for Packet
#include "otherstructs.h"	// Needed for Requested_Block_Struct
#include "Friend.h"		// Needed for CFriend
#include "ClientList.h"		// Needed for CClientList
#include "amule.h"		// Needed for theApp
#include "PartFile.h"		// Needed for CPartFile
#include "ListenSocket.h"	// Needed for CClientReqSocket
#include "opcodes.h"		// Needed for SOURCESSLOTS
#include "updownclient.h"	// Needed for CUpDownClient
#include "amuleIPV4Address.h"	// Needed for amuleIPV4Address

//#define NET_TEST 

// some client testing variables
static wxString crash_name   = "[Invalid User Name]"; 
static wxString empty_name = "[Empty User Name]";

/*
// prevent fscking dns queries
class amuleIPV4Address : public wxIPV4address {
public:
	amuleIPV4Address() : wxIPV4address() {};
#ifndef __WXMSW__
	virtual bool Hostname(unsigned long addr) {
		return GAddress_INET_SetHostAddress(m_address,addr)==GSOCK_NOERROR;
	};
	virtual bool Hostname(char* addr) {
		struct in_addr inaddr;
		inet_aton(addr,&inaddr);
		return GAddress_INET_SetHostAddress(m_address,inaddr.s_addr)==GSOCK_NOERROR;
	}
#endif
};
*/

//	members of CUpDownClient
//	which are used by down and uploading functions 

CUpDownClient::CUpDownClient(CClientReqSocket* sender)
{
	socket = sender;
	//printf("Socket %x set on client %x\n",socket, this);
	reqfile = 0;
	Init();
}

CUpDownClient::CUpDownClient(uint16 in_port, uint32 in_userid,uint32 in_serverip, uint16 in_serverport,CPartFile* in_reqfile)
{
	socket = NULL;
	//printf("Socket %x set on client %x\n",socket, this);
	Init();
	m_nUserID = in_userid;
	m_nUserPort = in_port;
	sourcesslot=m_nUserID%SOURCESSLOTS;
	if (!HasLowID()) {
		sprintf(m_szFullUserIP,"%i.%i.%i.%i",(uint8)m_nUserID,(uint8)(m_nUserID>>8),(uint8)(m_nUserID>>16),(uint8)(m_nUserID>>24));
	}
	m_dwServerIP = in_serverip;
	m_nServerPort = in_serverport;
	reqfile = in_reqfile;
	ReGetClientSoft();
}

void CUpDownClient::Init()
{
	memset(m_szFullUserIP,0,21);
	credits = 0;
	//memset(reqfileid, 0, sizeof reqfileid);
	memset(requpfileid, 0, sizeof requpfileid);
	// m_nAvDownDatarate = 0;  // unused
	m_byChatstate = 0;
	m_cShowDR = 0;
	
	m_cFailed = 0;
	m_dwBanTime = 0;
	m_nMaxSendAllowed = 0;
	m_nTransferedUp = 0;
	m_cSendblock = 0;
	m_cAsked = 0;
	m_cDownAsked = 0;
#ifdef DOWNLOADRATE_FILTERED
	msSentPrev = msReceivedPrev = 0;
	kBpsUp = kBpsDown = 0.0;
	fDownAvgFilter = 1.0;
	bytesReceivedCycle = 0;
#else
	m_nDownDatarate = 0;
	m_nDownDataRateMS = 0;
	m_nSumForAvgDownDataRate = 0;
#endif
	m_pszUsername = 0;
	m_dwUserIP = 0;
	m_nUserID = 0;
	m_nServerPort = 0;
	m_bBanned = false;
	//m_bFileListRequested = false;
	m_iFileListRequested = 0;
	m_dwLastUpRequest = 0;
	m_bEmuleProtocol = false;
	usedcompressiondown = false;
	m_bUsedComprUp = false;
	m_bCompleteSource = false;
	m_bFriendSlot = false;
	m_bCommentDirty = false;
	m_bReaskPending = false;
	m_bUDPPending = false;
	m_nUserPort = 0;
	m_nPartCount = 0;
	m_nUpPartCount = 0;
	m_abyPartStatus = 0;
	m_abyUpPartStatus = 0;
	m_dwLastAskedTime = 0;
	m_nDownloadState = DS_NONE;
	m_pszClientFilename = 0;
	m_dwUploadTime = 0;
	m_nTransferedDown = 0;
	m_nUploadState = US_NONE;
	m_dwLastBlockReceived = 0;




	m_nRemoteQueueRank = 0;
	m_dwLastSourceRequest = 0;
	m_dwLastSourceAnswer = 0;
	m_dwLastAskedForSources = 0;
	
	m_SecureIdentState = IS_UNAVAILABLE;
	m_dwLastSignatureIP = 0;
	
	m_byInfopacketsReceived = IP_NONE;	
	
	m_bIsHybrid = false;
	m_bIsNewMLD = false;
	m_bIsML = false;
	m_Friend = NULL;
	m_iRate=0;
	m_strComment="";
	m_nCurSessionUp = 0;
	m_clientSoft=SO_UNKNOWN;
	
	m_bRemoteQueueFull = false;
	memset( m_achUserHash, 0, 16);
	SetWaitStartTime();
	if (socket) {
		struct sockaddr_in sockAddr;
		memset(&sockAddr, 0, sizeof(sockAddr));
		wxIPV4address address;
		socket->GetPeer(address);
		//uint32 nSockAddrLen = sizeof(sockAddr);
		//socket->GetPeerName((SOCKADDR*)&sockAddr,(int*)&nSockAddrLen);
		sockAddr.sin_addr.s_addr = inet_addr(address.IPAddress().c_str());
		m_dwUserIP = sockAddr.sin_addr.s_addr;
		strcpy(m_szFullUserIP,inet_ntoa(sockAddr.sin_addr));
	}
	sourcesslot=0;
	m_fHashsetRequesting = 0;
	m_fSharedDirectories = 0;
	m_dwEnteredConnectedState = 0;
	// At the beginning, client is't a thief :)
	leechertype = 0;
	thief = false;
	m_lastPartAsked = 0xffff;
	m_nUpCompleteSourcesCount= 0;
	m_lastRefreshedDLDisplay = 0;
	m_bAddNextConnect = false;  // VQB Fix for LowID slots only on connection
	m_SoftLen = 0;
	m_bHelloAnswerPending = false;
	m_fSentCancelTransfer = 0;
	Extended_aMule_SO = 0;

	ClearHelloProperties();	
	
}	


CUpDownClient::~CUpDownClient()
{
	/* Razor 1a - Modif by MikaelB */
	if(reqfile != NULL) {
		reqfile->RemoveDownloadingSource(this);
	}
	/* End modif */
	//printf("1...");
	
	// Kry - This 'if' is not really needed because the client list gets 
	// deleted AFTER all the clients were removed by the listensocket destructor.
	if (theApp.clientlist) {
		theApp.clientlist->RemoveClient(this);
	}
	
	if (m_Friend) {
		m_Friend->m_LinkedClient = NULL;
		theApp.amuledlg->chatwnd->RefreshFriend(m_Friend);
		m_Friend = NULL;
	}
	//printf("2...");
	if (socket) {
		socket->client = 0; 
		socket->Safe_Delete();
	}
	//printf("3...");
	if (m_pszUsername) {
		delete[] m_pszUsername;
		m_pszUsername = NULL;
	}
	
	if (m_pszClientFilename) {
		delete[] m_pszClientFilename;
		m_pszClientFilename = NULL;
	}
	
	//printf("4...");
	if (m_abyPartStatus) {
		delete[] m_abyPartStatus;
		m_abyPartStatus = NULL;
	}
	//printf("5...");
	if (m_abyUpPartStatus) {
		delete[] m_abyUpPartStatus;
		m_abyUpPartStatus = NULL;
	}
	//printf("6...");
	ClearUploadBlockRequests();
	//printf("7...");
	for (POSITION pos = m_DownloadBlocks_list.GetHeadPosition();pos != 0;m_DownloadBlocks_list.GetNext(pos)) {
		delete m_DownloadBlocks_list.GetAt(pos);
	}
	//printf("8...");
	m_DownloadBlocks_list.RemoveAll();
	for (POSITION pos = m_RequestedFiles_list.GetHeadPosition();pos != 0;m_RequestedFiles_list.GetNext(pos)) {
		delete m_RequestedFiles_list.GetAt(pos);
	}
	//printf("9...");
	m_RequestedFiles_list.RemoveAll();
	for (POSITION pos = m_PendingBlocks_list.GetHeadPosition();pos != 0;m_PendingBlocks_list.GetNext(pos)) {
		Pending_Block_Struct *pending = m_PendingBlocks_list.GetAt(pos);
		delete pending->block;
		// Not always allocated
		if (pending->zStream) {
			inflateEnd(pending->zStream);
			delete pending->zStream;
		}
		delete pending;
	}
	/* eMule 0.30c manage this also, i give it a try ... (Creteil) */
	//printf("10...");
	for (POSITION pos =m_WaitingPackets_list.GetHeadPosition();pos != 0;m_WaitingPackets_list.GetNext(pos)) {
		delete m_WaitingPackets_list.GetAt(pos);
	}

	//printf("11...");
	if (m_iRate>0 || m_strComment.GetLength()>0) {
		m_iRate=0; 
		m_strComment.Empty();
		if (reqfile) {
			reqfile->UpdateFileRatingCommentAvail();
		}
	}

#ifndef DOWNLOADRATE_FILTERED
	m_AvarageDDR_list.RemoveAll();
#endif
	//DEBUG_ONLY (theApp.listensocket->Debug_ClientDeleted(this));
	SetUploadFileID(NULL);
	//printf("END\n");
}

void CUpDownClient::ClearHelloProperties()
{
	m_nUDPPort = 0;
	m_byUDPVer = 0;
	m_byDataCompVer = 0;
	m_byEmuleVersion = 0;
	m_bySourceExchangeVer = 0;
	m_byAcceptCommentVer = 0;
	m_byExtendedRequestsVer = 0;
	m_byCompatibleClient = 0;
	m_nKadPort = 0;
	m_bySupportSecIdent = 0;
	m_bSupportsPreview = 0;
	m_nClientVersion = 0;
	m_fSharedDirectories = 0;
	m_bMultiPacket = 0;
	m_SoftLen = 0;
}

bool CUpDownClient::ProcessHelloPacket(char* pachPacket, uint32 nSize)
{
	CSafeMemFile data((BYTE*)pachPacket,nSize);
	uint8 hashsize;
	if ( (1!=data.Read(hashsize)) ) {
		throw wxString(wxT("Invalid Hello packet: Short packet when reading hash size"));
	}
	if ( 16 != hashsize ) {
		/*
		 * Hint: We can not accept other sizes here because:
		 *       - the magic number is spread all over the source
		 *       - the answer packet lacks the size field
		 */
		wxASSERT(0);
		throw wxString(wxT("Invalid Hello packet: Other userhash sizes than 16 are not implemented"));
	}
	// eMule 0.42: reset all client properties; a client may not send a particular emule tag any longer
	ClearHelloProperties();	
	return ProcessHelloTypePacket(&data);
}

bool CUpDownClient::ProcessHelloAnswer(char* pachPacket, uint32 nSize)
{
	CSafeMemFile data((BYTE*)pachPacket,nSize);
	bool bIsMule = ProcessHelloTypePacket(&data);
	m_bHelloAnswerPending = false;
	return bIsMule;
}

bool CUpDownClient::ProcessHelloTypePacket(CSafeMemFile* data)
{
		
	m_bIsHybrid = false;
	m_bIsML = false;
	m_fNoViewSharedFiles = 0;
	DWORD dwEmuleTags = 0;
	
	try {	
	
		data->ReadHash16(m_achUserHash);
		data->Read(m_nUserID);
		uint16 nUserPort = 0;
		data->Read(nUserPort); // hmm clientport is sent twice - why?
		uint32 tagcount;
		data->Read(tagcount);
		for (uint32 i = 0;i < tagcount; i++){
			CTag temptag(data);
			switch(temptag.tag.specialtag){
				case CT_NAME:
					if (m_pszUsername){
						delete[] m_pszUsername;
						m_pszUsername = NULL; // needed, in case 'nstrdup' fires an exception!!
					}
					if( temptag.tag.stringvalue ) {
						m_pszUsername = nstrdup(temptag.tag.stringvalue);
					}
					break;
				case CT_VERSION:
					m_nClientVersion = temptag.tag.intvalue;
					break;
				case ET_MOD_VERSION:
					if (temptag.tag.type == 2) {
						m_strModVersion = temptag.tag.stringvalue;
					} else if (temptag.tag.type == 3) {
						m_strModVersion.Format(_T("ModID=%u"), temptag.tag.intvalue);						
					} else {
						m_strModVersion = _T("ModID=<Unknwon>");
					}
					break;			
				case CT_PORT:
					nUserPort = temptag.tag.intvalue;
					break;
				case CT_EMULE_UDPPORTS:
					// 16 KAD Port
					// 16 UDP Port
					m_nKadPort = (uint16)(temptag.tag.intvalue >> 16);
					m_nUDPPort = (uint16)temptag.tag.intvalue;
					dwEmuleTags |= 1;
					#ifdef __PACKET_DEBUG__
					printf("Hello type packet processing with eMule ports UDP=%i KAD=%i\n",m_nUDPPort,m_nKadPort);
					#endif
					break;				
				case CT_EMULE_MISCOPTIONS1:
					//  4 --Reserved for future use--
					//  4 UDP version
					//  4 Data compression version
					//  4 Secure Ident
					//  4 Source Exchange
					//  4 Ext. Requests
					//  4 Comments
					//	1 --Reserved for future use--
					//	1 No 'View Shared Files' supported
					//	1 MultiPacket
					//  1 Preview
					m_byUDPVer				= (temptag.tag.intvalue >> 4*6) & 0x0f;
					m_byDataCompVer			= (temptag.tag.intvalue >> 4*5) & 0x0f;
					m_bySupportSecIdent		= (temptag.tag.intvalue >> 4*4) & 0x0f;
					m_bySourceExchangeVer	= (temptag.tag.intvalue >> 4*3) & 0x0f;
					m_byExtendedRequestsVer	= (temptag.tag.intvalue >> 4*2) & 0x0f;
					m_byAcceptCommentVer	= (temptag.tag.intvalue >> 4*1) & 0x0f;
					m_fNoViewSharedFiles	= (temptag.tag.intvalue >> 1*2) & 0x01;
					m_bMultiPacket			= (temptag.tag.intvalue >> 1*1) & 0x01;
					m_fSupportsPreview		= (temptag.tag.intvalue >> 1*0) & 0x01;
					dwEmuleTags |= 2;
					#ifdef __PACKET_DEBUG__
					printf("Hello type packet processing with eMule Misc Options:\n");
					printf("m_byUDPVer = %i\n",m_byUDPVer);
					printf("m_byDataCompVer = %i\n",m_byDataCompVer);
					printf("m_bySupportSecIdent = %i\n",m_bySupportSecIdent);
					printf("m_bySourceExchangeVer = %i\n",m_bySourceExchangeVer);
					printf("m_byExtendedRequestsVer = %i\n",m_byExtendedRequestsVer);
					printf("m_byAcceptCommentVer = %i\n",m_byAcceptCommentVer);
					printf("m_fNoViewSharedFiles = %i\n",m_fNoViewSharedFiles);
					printf("m_bMultiPacket = %i\n",m_bMultiPacket);
					printf("m_fSupportsPreview = %i\n",m_fSharedDirectories);
					printf("That's all.\n");
					#endif					

					break;				
				case CT_EMULE_VERSION:
					//  8 Compatible Client ID
					//  7 Mjr Version (Doesn't really matter..)
					//  7 Min Version (Only need 0-99)
					//  3 Upd Version (Only need 0-5)
					//  7 Bld Version (Only need 0-99)
					m_byCompatibleClient = (temptag.tag.intvalue >> 24);
					m_nClientVersion = temptag.tag.intvalue & 0x00ffffff;
					m_byEmuleVersion = 0x99;
					m_fSharedDirectories = 1;
					dwEmuleTags |= 4;
					break;				
			}
		}
		
		m_nUserPort = nUserPort;
		data->Read(m_dwServerIP);
		data->Read(m_nServerPort);
		// Hybrid now has an extra uint32.. What is it for?
		// Also, many clients seem to send an extra 6? These are not eDonkeys or Hybrids..
		if ( data->GetLength() - data->GetPosition() == sizeof(uint32) ){
			// Kry - Changes on eMule code for compat.
			char test[4];
			// lemonfan - this is not an "normal" string, so wxString cant be used
			data->ReadRaw(&test, 4);
			if ((test[0]=='M') && (test[1]=='L') && (test[2]=='D') && (test[3]=='K')) {
				m_bIsML=true;
			} else{
				m_bIsHybrid = true;
				m_fSharedDirectories = 1;
			}
		}
		
	} catch ( CStrangePacket )
	{
		printf("\nWrong Tags on hello type packet!!\n");
		printf("Sent by %s on ip %s port %i using client %x version %x\n",GetUserName(),GetFullIP(),GetUserPort(),GetClientSoft(),GetVersion());
		printf("User Disconnected.\n");
		throw wxString("Wrong Tags on hello type packet");
	}
	catch ( CInvalidPacket (e))
	{
		printf("Wrong Tags on hello type packet - %s\n\n",e.what());
		printf("Sent by %s on ip %s port %i using client %x version %x\n",GetUserName(),GetFullIP(),GetUserPort(),GetClientSoft(),GetVersion());
		printf("User Disconnected.\n");		
		throw wxString("Wrong Tags on hello type packet");
	}
	/* Kry - Added the CT_EMULE_VERSION tag - probably no more need for this
	
	if( m_nClientVersion > 10000 && m_nClientVersion < 100000 )
		m_nClientVersion = m_nClientVersion - (m_nClientVersion/10000)*10000;
	if( m_nClientVersion > 1000 )
		m_nClientVersion = m_nClientVersion - (m_nClientVersion/1000)*1000;
	if( m_nClientVersion < 100 )
		m_nClientVersion *= 10;
	
	*/
	
	// tecxx 1609 2002 - add client's servet to serverlist (Moved to uploadqueue.cpp)

	if (socket) {
		struct sockaddr_in sockAddr;
		memset(&sockAddr, 0, sizeof(sockAddr));
		wxIPV4address address;
		socket->GetPeer(address);
		sockAddr.sin_addr.s_addr=inet_addr(address.IPAddress().c_str());
		m_dwUserIP = sockAddr.sin_addr.s_addr;
		strcpy(m_szFullUserIP,inet_ntoa(sockAddr.sin_addr));
	} else {
		printf("Huh, socket failure. Avoided crash this time.\n");
	}
	
	if (theApp.glob_prefs->AddServersFromClient()) {
		in_addr addhost;
		addhost.s_addr = m_dwServerIP;
		CServer* addsrv = new CServer(m_nServerPort, inet_ntoa(addhost));
		addsrv->SetListName(addsrv->GetAddress());
			if (!theApp.amuledlg->serverwnd->serverlistctrl->AddServer(addsrv, true)) {
				delete addsrv;
		}
	}

	if(!HasLowID() || m_nUserID == 0) {
		m_nUserID = m_dwUserIP;
	}

	// get client credits
	CClientCredits* pFoundCredits = theApp.clientcredits->GetCredit(m_achUserHash);
	if (credits == NULL){
		credits = pFoundCredits;
		if (!theApp.clientlist->ComparePriorUserhash(m_dwUserIP, m_nUserPort, pFoundCredits)){
			theApp.amuledlg->AddDebugLogLine(false, "Clients: %s (%s), Banreason: Userhash changed (Found in TrackedClientsList)", GetUserName(), GetFullIP()); 
			Ban();
		}	
	} else if (credits != pFoundCredits){
		// userhash change ok, however two hours "waittime" before it can be used
		credits = pFoundCredits;
		theApp.amuledlg->AddDebugLogLine(false, "Clients: %s (%s), Banreason: Userhash changed", GetUserName(),GetFullIP()); 
		Ban();
	}

	if ((m_Friend = theApp.amuledlg->chatwnd->FindFriend(m_achUserHash, m_dwUserIP, m_nUserPort)) != NULL){
		// Link the friend to that client
		if (m_Friend->m_LinkedClient){
			if (m_Friend->m_LinkedClient != this){
				bool bFriendSlot = m_Friend->m_LinkedClient->GetFriendSlot();
				// avoid that an unwanted client instance keeps a friend slot
				m_Friend->m_LinkedClient->SetFriendSlot(false);
				m_Friend->m_LinkedClient->m_Friend = NULL;
				m_Friend->m_LinkedClient = this;
				// move an assigned friend slot between different client instances which are/were also friends
				m_Friend->m_LinkedClient->SetFriendSlot(bFriendSlot);
			}
		} else {
			m_Friend->m_LinkedClient = this;
		}
		md4cpy(m_Friend->m_abyUserhash,GetUserHash());
		m_Friend->m_dwHasHash = md4cmp(m_Friend->m_abyUserhash, CFriend::sm_abyNullHash) ? 1 : 0;
		m_Friend->m_strName = m_pszUsername;
		m_Friend->m_dwLastUsedIP = m_dwUserIP;
		m_Friend->m_nLastUsedPort = m_nUserPort;
		m_Friend->m_dwLastSeen = time(NULL);
		theApp.amuledlg->chatwnd->RefreshFriend(m_Friend);
	}
	else{
		// avoid that an unwanted client instance keeps a friend slot
		SetFriendSlot(false);
	}

	
	// We want to educate Users of major comercial GPL breaking mods by telling them about the effects
	// check for known advertising in usernames
	// the primary aim is not to technical block those but to make users use a GPL-conform version
	CString strBuffer = m_pszUsername;
	strBuffer.MakeUpper();
	strBuffer.Remove(' ');
	if (strBuffer.Find("EMULE-CLIENT") != -1 || strBuffer.Find("POWERMULE") != -1){
		m_bGPLEvildoer = true;  
	}

	ReGetClientSoft();
	
	m_byInfopacketsReceived |= IP_EDONKEYPROTPACK;

	// check if at least CT_EMULEVERSION was received, all other tags are optional
	bool bIsMule = (dwEmuleTags & 0x04) == 0x04;
	if (bIsMule) {
		m_bEmuleProtocol = true;
		m_byInfopacketsReceived |= IP_EMULEPROTPACK;
	}


	#ifdef __USE_KAD__
	if( GetKadPort() && Kademlia::CKademlia::isRunning() )
	{
		Kademlia::CKademlia::getUDPListener()->bootstrap(ntohl(GetIP()), GetKadPort());
	}
	#endif

	return bIsMule;
}


bool CUpDownClient::SendHelloPacket() {

	if (socket == NULL){
		wxASSERT(0);
		return true;
	}
	
	// if IP is filtered, dont greet him but disconnect...
	struct sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr));
	//uint32 nSockAddrLen = sizeof(sockAddr);
	//socket->GetPeerName((SOCKADDR*)&sockAddr,(int*)&nSockAddrLen);
	wxIPV4address address;
	socket->GetPeer(address);
	sockAddr.sin_addr.s_addr = inet_addr(address.IPAddress().c_str());
	if ( theApp.ipfilter->IsFiltered(sockAddr.sin_addr.s_addr)) {
		theApp.amuledlg->AddDebugLogLine(true,CString(_("Filtered IP: %s (%s)")).GetData(),GetFullIP(),theApp.ipfilter->GetLastHit().GetData());
		theApp.stat_filteredclients++;
		if (Disconnected("IPFilter")) {
			delete this;
			return false;
		}
		return true;
	}

	CSafeMemFile data(128);
	data.Write((uint8)16); // size of userhash
	SendHelloTypePacket(&data);
	Packet* packet = new Packet(&data);
	packet->opcode = OP_HELLO;
	theApp.uploadqueue->AddUpDataOverheadOther(packet->size);
	socket->SendPacket(packet,true);
	m_bHelloAnswerPending = true;
	return true;
}

void CUpDownClient::SendMuleInfoPacket(bool bAnswer) {

	if (socket == NULL){
		wxASSERT(0);
		return;
	}
	
	CMemFile* data = new CMemFile();
	data->Write((uint8)CURRENT_VERSION_SHORT);
	data->Write((uint8)EMULE_PROTOCOL);

	// Support for ET_MOD_VERSION [BlackRat]
	data->Write((uint32)9); 

	CTag tag1(ET_COMPRESSION,1);
	tag1.WriteTagToFile(data);
	CTag tag2(ET_UDPVER,4);
	tag2.WriteTagToFile(data);
	CTag tag3(ET_UDPPORT,theApp.glob_prefs->GetUDPPort());
	tag3.WriteTagToFile(data);
	CTag tag4(ET_SOURCEEXCHANGE,2);
	tag4.WriteTagToFile(data);
	CTag tag5(ET_COMMENTS,1);
	tag5.WriteTagToFile(data);
	CTag tag6(ET_EXTENDEDREQUEST,2);
	tag6.WriteTagToFile(data);
	
	uint32 dwTagValue = (theApp.clientcredits->CryptoAvailable() ? 3 : 0);
	// Kry - Needs the preview code from eMule	
	/*
	// set 'Preview supported' only if 'View Shared Files' allowed
	if (theApp.glob_prefs->CanSeeShares() != vsfaNobody) {
		dwTagValue |= 128;
	}
	*/
	CTag tag7(ET_FEATURES, dwTagValue);
	tag7.WriteTagToFile(data);
	
	CTag tag8(ET_COMPATIBLECLIENT,SO_AMULE);
	tag8.WriteTagToFile(data);

	// Support for tag ET_MOD_VERSION
	wxString mod_name(MOD_VERSION_LONG);
	CTag tag9(ET_MOD_VERSION, mod_name);
	tag9.WriteTagToFile(data);
	// Maella end

	Packet* packet = new Packet(data,OP_EMULEPROT);
	delete data;
	if (!bAnswer)
		packet->opcode = OP_EMULEINFO;
	else
		packet->opcode = OP_EMULEINFOANSWER;
	if (socket) {
		theApp.uploadqueue->AddUpDataOverheadOther(packet->size);
		socket->SendPacket(packet,true,true);
	}
}

void CUpDownClient::ProcessMuleInfoPacket(char* pachPacket, uint32 nSize)
{

	try {
		
		//DumpMem(pachPacket,nSize);
		CSafeMemFile data((BYTE*)pachPacket,nSize);
		m_byCompatibleClient = 0;
		//The version number part of this packet will soon be useless since it is only able to go to v.99.
		//Why the version is a uint8 and why it was not done as a tag like the eDonkey hello packet is not known..
		//Therefore, sooner or later, we are going to have to switch over to using the eDonkey hello packet to set the version.
		//No sense making a third value sent for versions..
		data.Read(m_byEmuleVersion);
		if( m_byEmuleVersion == 0x2B ) {
			m_byEmuleVersion = 0x22;
		}	
		uint8 protversion;
		data.Read(protversion);

		//implicitly supported options by older clients
		if (protversion == EMULE_PROTOCOL) {
			//in the future do not use version to guess about new features

			if(m_byEmuleVersion < 0x25 && m_byEmuleVersion > 0x22)
				m_byUDPVer = 1;

			if(m_byEmuleVersion < 0x25 && m_byEmuleVersion > 0x21)
				m_bySourceExchangeVer = 1;		

			if(m_byEmuleVersion == 0x24)
				m_byAcceptCommentVer = 1;

			// Shared directories are requested from eMule 0.28+ because eMule 0.27 has a bug in 
			// the OP_ASKSHAREDFILESDIR handler, which does not return the shared files for a 
			// directory which has a trailing backslash.
			if(m_byEmuleVersion >= 0x28 && !m_bIsML) // MLdonkey currently does not support shared directories
				m_fSharedDirectories = 1;

		} else {
			return;
		}	
		
		m_bEmuleProtocol = true;

		uint32 tagcount;
		data.Read(tagcount);
		
		for (uint32 i = 0;i < tagcount; i++){
			CTag temptag(&data);
			switch(temptag.tag.specialtag){
				case ET_COMPRESSION:
					// Bits 31- 8: 0 - reserved
					// Bits  7- 0: data compression version
					m_byDataCompVer = temptag.tag.intvalue;
					break;
				case ET_UDPPORT:
					// Bits 31-16: 0 - reserved
					// Bits 15- 0: UDP port
					m_nUDPPort = temptag.tag.intvalue;
					break;
				case ET_UDPVER:
					// Bits 31- 8: 0 - reserved
					// Bits  7- 0: UDP protocol version
					m_byUDPVer = temptag.tag.intvalue;
					break;
				case ET_SOURCEEXCHANGE:
					// Bits 31- 8: 0 - reserved
					// Bits  7- 0: source exchange protocol version
					m_bySourceExchangeVer = temptag.tag.intvalue;
					break;
				case ET_COMMENTS:
					// Bits 31- 8: 0 - reserved
					// Bits  7- 0: comments version
					m_byAcceptCommentVer = temptag.tag.intvalue;
					break;
				case ET_EXTENDEDREQUEST:
						// Bits 31- 8: 0 - reserved
					// Bits  7- 0: extended requests version
					m_byExtendedRequestsVer = temptag.tag.intvalue;
					break;
				case ET_COMPATIBLECLIENT:
					// Bits 31- 8: 0 - reserved
					// Bits  7- 0: compatible client ID
					m_byCompatibleClient = temptag.tag.intvalue;
					break;
				case ET_FEATURES:
					// Bits 31- 8: 0 - reserved
					// Bit	    7: Preview
					// Bit   6- 0: secure identification
					m_bySupportSecIdent = temptag.tag.intvalue & 3;
					m_bSupportsPreview = (temptag.tag.intvalue & 128) > 0;
					break;
				case ET_MOD_VERSION:
					if (temptag.tag.type == 2) {
						m_strModVersion = temptag.tag.stringvalue;
					}
					else if (temptag.tag.type == 3) {
						m_strModVersion.Format(_T("ModID=%u"), temptag.tag.intvalue);
					}
					else {
						m_strModVersion = _T("ModID=<Unknwon>");
					}
					CheckForGPLEvilDoer();
					break;
				default:
					//printf("Mule Unk Tag 0x%02x=%x\n", temptag.tag.specialtag, (UINT)temptag.tag.intvalue);
					break;
			}
		}
	}
	catch ( CStrangePacket )
	{
		printf("\nWrong Tags on Mule Info packet!!\n");
		printf("Sent by %s on ip %s port %i using client %x version %x\n",GetUserName(),GetFullIP(),GetUserPort(),GetClientSoft(),GetMuleVersion());
		printf("User Disconnected.\n");
		printf("Packet Dump:\n");
		DumpMem(pachPacket,nSize);
		throw wxString("Wrong Tags on Mule Info packet");
	}
	catch ( CInvalidPacket (e))
	{
		printf("Wrong Tags on Mule Info packet - %s\n\n",e.what());
		printf("Sent by %s on ip %s port %i using client %x version %x\n",GetUserName(),GetFullIP(),GetUserPort(),GetClientSoft(),GetMuleVersion());
		printf("User Disconnected.\n");		
		printf("Packet Dump:\n");		
		DumpMem(pachPacket,nSize);
		throw wxString("Wrong Tags on Mule Info packet");
	}
	
	if( m_byDataCompVer == 0 ){
		m_bySourceExchangeVer = 0;
		m_byExtendedRequestsVer = 0;
		m_byAcceptCommentVer = 0;
		m_nUDPPort = 0;
	}

	ReGetClientSoft();

	m_byInfopacketsReceived |= IP_EMULEPROTPACK;

}

void CUpDownClient::SendHelloAnswer()
{

	if (socket == NULL){
		wxASSERT(0);
		return;
	}	
	
	CSafeMemFile data(128);
	SendHelloTypePacket(&data);
	Packet* packet = new Packet(&data);
	packet->opcode = OP_HELLOANSWER;

	theApp.uploadqueue->AddUpDataOverheadOther(packet->size);
	socket->SendPacket(packet,true);

}

void CUpDownClient::SendHelloTypePacket(CMemFile* data)
{
	data->WriteHash16(theApp.glob_prefs->GetUserHash());
	data->Write((uint32)theApp.serverconnect->GetClientID());
	data->Write((uint16)theApp.glob_prefs->GetPort());

	#ifdef __CVS__
	// Kry - This is the tagcount!!! Be sure to update it!!
	data->Write((uint32)6);
	#else
	data->Write((uint32)5);  // NO MOD_VERSION
	#endif
	
	
	CTag tagname(CT_NAME,theApp.glob_prefs->GetUserNick());
	tagname.WriteTagToFile(data);
	
	CTag tagversion(CT_VERSION,EDONKEYVERSION);
	tagversion.WriteTagToFile(data);
	// eMule UDP Ports
	
	uint32 kadUDPPort = 0;
	#ifdef __USE_KAD__
	if(Kademlia::CKademlia::isConnected())
	{
		kadUDPPort = theApp.glob_prefs->GetUDPPort();
	}
	#endif
	CTag tagUdpPorts(CT_EMULE_UDPPORTS, 
				(kadUDPPort									<< 16) |
				((uint32)theApp.glob_prefs->GetUDPPort()         ) ); 
	tagUdpPorts.WriteTagToFile(data);
	
	// aMule Version
	CTag tagMuleVersion(CT_EMULE_VERSION, 
				(SO_AMULE	<< 24) |
				(VERSION_MJR			<< 17) |
				(VERSION_MIN			<< 10) |
				(VERSION_UPDATE			<<  7)//|
				//(RESERVED			     ) 
				);
	tagMuleVersion.WriteTagToFile(data);	


	// eMule Misc. Options #1
	const UINT uUdpVer				= 4;
	const UINT uDataCompVer			= 1;
	const UINT uSupportSecIdent		= theApp.clientcredits->CryptoAvailable() ? 3 : 0;
	const UINT uSourceExchangeVer	= 2; //3; Kry - Our source exchange it type 2, TODO
	const UINT uExtendedRequestsVer	= 2;
	const UINT uAcceptCommentVer	= 1;
	const UINT uNoViewSharedFiles	= (theApp.glob_prefs->CanSeeShares() == vsfaNobody) ? 1 : 0; // for backward compatibility this has to be a 'negative' flag
	const UINT uMultiPacket			= 1;
	const UINT uSupportPreview		= 0; //(thePrefs.CanSeeShares() != vsfaNobody) ? 1 : 0; // set 'Preview supported' only if 'View Shared Files' allowed
	CTag tagMisOptions(CT_EMULE_MISCOPTIONS1, 
//				(						<< 4*7) |
				(uUdpVer				<< 4*6) |
				(uDataCompVer			<< 4*5) |
				(uSupportSecIdent		<< 4*4) |
				(uSourceExchangeVer		<< 4*3) |
				(uExtendedRequestsVer	<< 4*2) |
				(uAcceptCommentVer		<< 4*1) |
//				(						<< 1*3) |
				(uNoViewSharedFiles		<< 1*2) |
				(uMultiPacket			<< 1*1) |
				(uSupportPreview		<< 1*0) );
	tagMisOptions.WriteTagToFile(data);

#ifdef __CVS__
	wxString mod_name(MOD_VERSION_SHORT);
	CTag tagModName(ET_MOD_VERSION, mod_name);
	tagModName.WriteTagToFile(data);
#endif
	
	uint32 dwIP = 0;
	uint16 nPort = 0;
	if (theApp.serverconnect->IsConnected()) {
		dwIP = theApp.serverconnect->GetCurrentServer()->GetIP();
		nPort = theApp.serverconnect->GetCurrentServer()->GetPort();
	}
	data->Write(dwIP);
	data->Write(nPort);
}


void CUpDownClient::ProcessMuleCommentPacket(char* pachPacket, uint32 nSize)
{
	try
	{
		if (!reqfile) {
			throw CInvalidPacket("comment packet for unknown file");
		}

		CSafeMemFile data((BYTE*)pachPacket,nSize);
		uint32 length;
		if ( sizeof(m_iRate) != data.Read(m_iRate) )
			throw CInvalidPacket("short packet reading rating");
		if ( sizeof(length) != data.Read(length) )
			throw CInvalidPacket("short packet reading comment length");
		
		reqfile->SetHasRating(true);
		theApp.amuledlg->AddDebugLogLine(false,_("Rating for file '%s' received: %i"),m_pszClientFilename,m_iRate);
		if (length>50) length=50;
		if (length>0){
			#warning Lacks Comment Filtering
			char* desc=new char[length+1];
			memset(desc,0,length+1);
			if ( (unsigned int)length != data.ReadRaw(desc,length) ) {
				throw CInvalidPacket("short packet reading comment string");
			}
			theApp.amuledlg->AddDebugLogLine(false,_("Description for file '%s' received: %s"), m_pszClientFilename, desc);
			m_strComment.Format("%s",desc);				
			theApp.amuledlg->AddDebugLogLine(false,_("Description for file '%s' received: %s"), m_pszClientFilename, m_strComment.GetData());
			reqfile->SetHasComment(true);
			delete[] desc;
		}

	}
	catch ( CStrangePacket )
	{
		printf("\nInvalid MuleComment packet!\n");
		printf("Sent by %s on ip %s port %i using client %i version %i\n",GetUserName(),GetFullIP(),GetUserPort(),GetClientSoft(),GetMuleVersion());
		printf("User Disconnected.\n");
		return;
		throw wxString("Wrong MuleComment packet");
	}
	catch ( CInvalidPacket (e))
	{
		printf("\nInvalid MuleComment packet - %s\n\n",e.what());
		printf("Sent by %s on ip %s port %i using client %i version %i\n",GetUserName(),GetFullIP(),GetUserPort(),GetClientSoft(),GetMuleVersion());
		printf("User Disconnected.\n");		
		throw wxString("Wrong MuleComment packet");
		return;
	}		

	if (reqfile->HasRating() || reqfile->HasComment()) { 
		theApp.amuledlg->transferwnd->downloadlistctrl->UpdateItem(reqfile);
	}
}

void CUpDownClient::ClearDownloadBlockRequests()
{
	for (POSITION pos = m_DownloadBlocks_list.GetHeadPosition();pos != 0;){
		Requested_Block_Struct* cur_block = m_DownloadBlocks_list.GetNext(pos);
		if (reqfile){
			reqfile->RemoveBlockFromList(cur_block->StartOffset,cur_block->EndOffset);
		}
		delete cur_block;
	}
	m_DownloadBlocks_list.RemoveAll();

	for (POSITION pos = m_PendingBlocks_list.GetHeadPosition();pos != 0;){
		Pending_Block_Struct *pending = m_PendingBlocks_list.GetNext(pos);
		if (reqfile){
			reqfile->RemoveBlockFromList(pending->block->StartOffset, pending->block->EndOffset);
		}

		delete pending->block;
		// Not always allocated
		if (pending->zStream){
			inflateEnd(pending->zStream);
			delete pending->zStream;
		}
		delete pending;
	}
	m_PendingBlocks_list.RemoveAll();
}

bool CUpDownClient::Disconnected(CString strReason, bool bFromSocket){
	//If this is a KAD client object, just delete it!
	//wxASSERT(theApp.clientlist->IsValidClient(this));

	#ifdef __USE_KAD__
	SetKadState(KS_NONE);
	#endif
	
	if (GetUploadState() == US_UPLOADING) {
		theApp.uploadqueue->RemoveFromUploadQueue(this);
	}
	
	if (GetDownloadState() == DS_DOWNLOADING) {
		SetDownloadState(DS_ONQUEUE);
	}
	else{
		// ensure that all possible block requests are removed from the partfile
		ClearDownloadBlockRequests();

		if(GetDownloadState() == DS_CONNECTED){
		    // client didn't responsed to our request for some reasons (remotely banned?)
		    // or it just doesn't has this file, so try to swap first
		    if (!SwapToAnotherFile(true, true, true, NULL)){
			    theApp.downloadqueue->RemoveSource(this);
			    //DEBUG_ONLY(AddDebugLogLine(false, "Removed %s from downloadqueue - didn't responsed to filerequests",GetUserName()));
		    }
	    }
	}
		
	// The remote client does not have to answer with OP_HASHSETANSWER *immediatly* 
	// after we've sent OP_HASHSETREQUEST. It may occure that a (buggy) remote client 
	// is sending use another OP_FILESTATUS which would let us change to DL-state to DS_ONQUEUE.
	if (((GetDownloadState() == DS_REQHASHSET) || m_fHashsetRequesting) && (reqfile)) {
        reqfile->hashsetneeded = true;
	}

	//wxASSERT(theApp.clientlist->IsValidClient(this));

	//check if this client is needed in any way, if not delete it
	bool bDelete = true;
	switch(m_nUploadState){
		case US_ONUPLOADQUEUE:
			bDelete = false;
			break;
	};
	switch(m_nDownloadState){
		case DS_ONQUEUE:
		case DS_TOOMANYCONNS:
		case DS_NONEEDEDPARTS:
		case DS_LOWTOLOWIP:
			bDelete = false;
	};

	switch(m_nUploadState){
		case US_CONNECTING:
		case US_WAITCALLBACK:
		case US_ERROR:
			bDelete = true;
	};
	switch(m_nDownloadState){
		case DS_CONNECTING:
		case DS_WAITCALLBACK:
		case DS_ERROR:
			bDelete = true;
	};
	

	if (GetChatState() != MS_NONE){
		bDelete = false;
		theApp.amuledlg->chatwnd->ConnectionResult(this,false);
	}
	
	if (!bFromSocket && socket){
		wxASSERT (theApp.listensocket->IsValidSocket(socket));
		socket->Safe_Delete();
	}
	
	socket = 0;
	
    if (m_iFileListRequested){
		theApp.amuledlg->AddLogLine(true,CString("Failed to retrieve shared files from %s"),GetUserName());
		m_iFileListRequested = 0;
	}
	if (m_Friend) {
		theApp.amuledlg->chatwnd->RefreshFriend(m_Friend);
	}
	//theApp.amuledlg->transferwnd->clientlistctrl.RefreshClient(this);

	if (bDelete){
		#ifdef __USE_DEBUG__
		if (thePrefs.GetDebugClientTCPLevel() > 0) {
			Debug("--- Deleted client            %s; Reason=%s\n", DbgGetClientInfo(true), strReason);
		}
		#endif
		return true;
	}
	else{
		#ifdef __USE_DEBUG__
		if (thePrefs.GetDebugClientTCPLevel() > 0) {
			Debug("--- Disconnected client       %s; Reason=%s\n", DbgGetClientInfo(true), strReason);
		}
		#endif
		m_fHashsetRequesting = 0;
		SetSentCancelTransfer(0);
		m_bHelloAnswerPending = false;
		return false;
	}
}

//Returned bool is not if the TryToConnect is successful or not..
//false means the client was deleted!
//true means the client was not deleted!
bool CUpDownClient::TryToConnect(bool bIgnoreMaxCon)
{
	if (theApp.listensocket->TooManySockets() && !bIgnoreMaxCon )  {
		if (!socket) { 
			if(Disconnected("Too many connections")) {
				delete this;
				return false;
			}
			return true;			
		} else if (!socket->IsConnected()) {
			if(Disconnected("Too many connections")) {
				delete this;
				return false;
			}
			return true;			
		}
	}
	
	#ifdef __USE_KAD__
	if( GetKadState() == KS_QUEUED_FWCHECK ) {
		SetKadState(KS_CONNECTING_FWCHECK);	
	}
	#endif
	
	#ifdef __USE_KAD__
	if (HasLowID() && !theApp.DoCallback(this)) {
	#else
	if (HasLowID() && (theApp.serverconnect->GetClientID() < 16777216)) {
	#endif
		if (GetDownloadState() == DS_CONNECTING) {
			SetDownloadState(DS_LOWTOLOWIP);
		} else if (GetDownloadState() == DS_REQHASHSET) {
			SetDownloadState(DS_ONQUEUE);
			reqfile->hashsetneeded = true;
		}
		if (GetUploadState() == US_CONNECTING) {
			if(Disconnected("LowID->LowID and US_CONNECTING")) {
				delete this;
				return false;
			}
		}
		return true;
	}

	if (!socket) {
		socket = new CClientReqSocket(theApp.glob_prefs,this);
	//	printf("Socket %x set on client %x\n",socket, this);
		if (!socket->Create()) {
			socket->Safe_Delete();
			return true;
		} 
	} else if (!socket->IsConnected()) {
		socket->Safe_Delete();
		socket = new CClientReqSocket(theApp.glob_prefs,this);
	//	printf("Socket %x set on client %x\n",socket, this);
		if (!socket->Create()) {
			socket->Safe_Delete();
			return true;
		}
	} else {
		ConnectionEstablished();
		return true;
	}
	if (HasLowID()) {
		if (GetDownloadState() == DS_CONNECTING) {
			SetDownloadState(DS_WAITCALLBACK);
		}
		if (GetUploadState() == US_CONNECTING) {
			if(Disconnected("LowID and US_CONNECTING"))
			{
				delete this;
				return false;
			}
			return true;
		}

		if (theApp.serverconnect->IsLocalServer(m_dwServerIP,m_nServerPort)) {
			CMemFile data;
			data.Write(m_nUserID);
			Packet* packet = new Packet(&data);
			packet->opcode = OP_CALLBACKREQUEST;

//			Packet* packet = new Packet(OP_CALLBACKREQUEST,4);
//			memcpy(packet->pBuffer,&m_nUserID,4);

			theApp.uploadqueue->AddUpDataOverheadServer(packet->size);
			theApp.serverconnect->SendPacket(packet);
		} else {
			if (GetUploadState() == US_NONE && (!GetRemoteQueueRank() || m_bReaskPending)) {
				theApp.downloadqueue->RemoveSource(this);
				if(Disconnected("LowID and US_NONE and QR=0"))
				{
					delete this;
					return false;
				}
				return true;
			} else {
				if (GetDownloadState() == DS_WAITCALLBACK) {
					m_bReaskPending = true;
					SetDownloadState(DS_ONQUEUE);
				}
			}
		}
	} else {
		amuleIPV4Address tmp;
		tmp.Hostname(GetFullIP());
		tmp.Service(GetUserPort());
		socket->Connect(tmp,FALSE);
		// socket->Connect(GetFullIP(),GetUserPort());
		if (!SendHelloPacket()) {
			return false; // client was deleted!
		}
	}
	return true;
}

void CUpDownClient::ConnectionEstablished()
{
// 0.42e
	#ifdef __USE_KAD__
	switch(GetKadState())
	{
		case KS_CONNECTING_FWCHECK:
            SetKadState(KS_CONNECTED_FWCHECK);
			break;
		case KS_QUEUED_BUDDY:
			SetKadState(KS_CONNECTED_BUDDY);
			break;
	}
	#endif	
	
	// ok we have a connection, lets see if we want anything from this client
	if (GetChatState() == MS_CONNECTING || GetChatState() == MS_CHATTING) {
		theApp.amuledlg->chatwnd->ConnectionResult(this,true);
	}

	switch(GetDownloadState()) {
		case DS_CONNECTING:
		case DS_WAITCALLBACK:
			m_bReaskPending = false;
			SetDownloadState(DS_CONNECTED);
			SendFileRequest();
	}
	if (m_bReaskPending){
		m_bReaskPending = false;
		if (GetDownloadState() != DS_NONE && GetDownloadState() != DS_DOWNLOADING) {
			SetDownloadState(DS_CONNECTED);
			SendFileRequest();
		}
	}
	switch(GetUploadState()){
		case US_CONNECTING:
		case US_WAITCALLBACK:
			if (theApp.uploadqueue->IsDownloading(this)) {
				SetUploadState(US_UPLOADING);
				Packet* packet = new Packet(OP_ACCEPTUPLOADREQ,0);
				theApp.uploadqueue->AddUpDataOverheadFileRequest(packet->size);
				socket->SendPacket(packet,true);
			}
	}
	if (m_iFileListRequested == 1) {
		Packet* packet = new Packet(m_fSharedDirectories ? OP_ASKSHAREDDIRS : OP_ASKSHAREDFILES,0);
		theApp.uploadqueue->AddUpDataOverheadOther(packet->size);
		socket->SendPacket(packet,true,true);
	}
	while (!m_WaitingPackets_list.IsEmpty()) {
		socket->SendPacket(m_WaitingPackets_list.RemoveHead());
	}
}

int CUpDownClient::GetHashType() const
{
	if (m_achUserHash[5] == 13 && m_achUserHash[14] == 110)
		return SO_OLDEMULE;
	else if (m_achUserHash[5] == 14 && m_achUserHash[14] == 111)
		return SO_EMULE;
 	else if (m_achUserHash[5] == 'M' && m_achUserHash[14] == 'L')
		return SO_MLDONKEY;
	else
		return SO_UNKNOWN;
}

void CUpDownClient::ReGetClientSoft()
{

	if (!m_pszUsername) {
		m_clientSoft=SO_UNKNOWN;
		m_clientVerString = _("Unknown");
		m_SoftLen = m_clientVerString.Length();
		return;
	}

	int iHashType = GetHashType();
	if (iHashType == SO_EMULE) {
		switch(m_byCompatibleClient){
			case SO_CDONKEY:
				m_clientSoft = SO_CDONKEY;
				m_clientVerString = _("cDonkey");
				break;
			case SO_LXMULE:
				m_clientSoft = SO_LXMULE;
				if(GetClientModString().IsEmpty() == false) {
					m_clientVerString = wxString::Format(("xMule %s"), GetClientModString().c_str());
				} else {
					m_clientVerString = _("xMule");
				}
				if (GetMuleVersion() > 0x26) {
					m_clientVerString += wxString::Format(" (Fake eMule version %x)",GetMuleVersion());
				}
				break;
			case SO_AMULE:
				m_clientSoft = SO_AMULE;
				if(GetClientModString().IsEmpty() == false) {
					Extended_aMule_SO &= 2;
					m_clientVerString = wxString::Format("aMule %s", GetClientModString().c_str());
				} else {
					m_clientVerString = _("aMule");
				}
				break;
			case SO_SHAREAZA:
				m_clientSoft = SO_SHAREAZA;
				m_clientVerString = _("Shareaza");
				break;
			case SO_MLDONKEY:
				m_clientSoft = SO_MLDONKEY;
				m_clientVerString = _("Old MlDonkey");
				break;
			case SO_NEW_MLDONKEY:
			case SO_NEW2_MLDONKEY:
				m_clientSoft = SO_NEW_MLDONKEY;
				m_clientVerString = _("New MlDonkey");
				break;		
			case SO_LPHANT:
				m_clientSoft = SO_LPHANT;
				m_clientVerString = _("lphant");
				break;					
			default:
				if (m_bIsML){
					m_clientSoft = SO_MLDONKEY;
					m_clientVerString = _("MLdonkey");
				}
				else if (m_bIsHybrid){
					m_clientSoft = SO_EDONKEYHYBRID;
					m_clientVerString = _("eDonkeyHybrid");
				}
				else if (m_byCompatibleClient != 0){
					m_clientSoft = SO_COMPAT_UNK;
					#ifdef __DEBUG__
					printf("Compatible client found with ET_COMPATIBLECLIENT of 0x%x\n",m_byCompatibleClient);
					#endif
					m_clientVerString = wxString::Format(_("eMule Compat(0x%x)"),m_byCompatibleClient);
				}
				else {
					// If we step here, it might mean 2 things:
					// a eMule
					// a Compat Client that has sent no MuleInfo packet yet.
					m_clientSoft = SO_EMULE;
					m_clientVerString = _("eMule");
				}
		}	
		
		m_SoftLen = m_clientVerString.Length();
		
		if (m_byEmuleVersion == 0){
			m_nClientVersion = MAKE_CLIENT_VERSION(0,0,0);
		} else if (m_byEmuleVersion != 0x99) {		
			UINT nClientMinVersion = (m_byEmuleVersion >> 4)*10 + (m_byEmuleVersion & 0x0f);
			m_nClientVersion = MAKE_CLIENT_VERSION(0,nClientMinVersion,0);
			if (m_clientSoft == SO_AMULE) {
				Extended_aMule_SO = 1; // no CVS flag for 1.x, so no &= right now
				m_clientVerString += wxString::Format(" 1.x (based on eMule v0.%u)", nClientMinVersion);
			} else {
				m_clientVerString +=  wxString::Format(" v0.%u", nClientMinVersion);
			}
		} else {					
			UINT nClientMajVersion = (m_nClientVersion >> 17) & 0x7f;
			UINT nClientMinVersion = (m_nClientVersion >> 10) & 0x7f;
			UINT nClientUpVersion  = (m_nClientVersion >>  7) & 0x07;
		
			m_nClientVersion = MAKE_CLIENT_VERSION(nClientMajVersion, nClientMinVersion, nClientUpVersion);
			
			if (m_clientSoft == SO_AMULE) {				
				m_clientVerString +=  wxString::Format(" v%u.%u.%u", nClientMajVersion, nClientMinVersion, nClientUpVersion);						
			} else {
				m_clientVerString +=  wxString::Format(" v%u.%u%c", nClientMajVersion, nClientMinVersion, 'a' + nClientUpVersion);
			}			
		}
		return;
	}		

	if (m_bIsHybrid){
		m_clientSoft = SO_EDONKEYHYBRID;
		// seen:
		// 105010	50.10
		// 10501	50.1
		// 1051		51.0
		// 501		50.1

		UINT nClientMajVersion;
		UINT nClientMinVersion;
		UINT nClientUpVersion;
		if (m_nClientVersion > 100000){
			UINT uMaj = m_nClientVersion/100000;
			nClientMajVersion = uMaj - 1;
			nClientMinVersion = (m_nClientVersion - uMaj*100000) / 100;
			nClientUpVersion = m_nClientVersion % 100;
		}
		else if (m_nClientVersion > 10000){
			UINT uMaj = m_nClientVersion/10000;
			nClientMajVersion = uMaj - 1;
			nClientMinVersion = (m_nClientVersion - uMaj*10000) / 10;
			nClientUpVersion = m_nClientVersion % 10;
		}
		else if (m_nClientVersion > 1000){
			UINT uMaj = m_nClientVersion/1000;
			nClientMajVersion = uMaj - 1;
			nClientMinVersion = m_nClientVersion - uMaj*1000;
			nClientUpVersion = 0;
		}
		else if (m_nClientVersion > 100){
			UINT uMin = m_nClientVersion/10;
			nClientMajVersion = 0;
			nClientMinVersion = uMin;
			nClientUpVersion = m_nClientVersion - uMin*10;
		}
		else{
			nClientMajVersion = 0;
			nClientMinVersion = m_nClientVersion;
			nClientUpVersion = 0;
		}
		m_nClientVersion = MAKE_CLIENT_VERSION(nClientMajVersion, nClientMinVersion, nClientUpVersion);

		m_clientVerString = "eDonkeyHybrid";

		m_SoftLen = m_clientVerString.Length();		
		
		if (nClientUpVersion) {
			m_clientVerString += wxString::Format(" v%u.%u.%u", nClientMajVersion, nClientMinVersion, nClientUpVersion);
		} else {
			m_clientVerString += wxString::Format(" v%u.%u", nClientMajVersion, nClientMinVersion);
		}
	
		return;
	}

	if (m_bIsML || (iHashType == SO_MLDONKEY)){
		m_clientSoft = SO_MLDONKEY;
		UINT nClientMinVersion = m_nClientVersion;
		m_nClientVersion = MAKE_CLIENT_VERSION(0, nClientMinVersion, 0);
		m_clientVerString = "MLdonkey";
		m_SoftLen = m_clientVerString.Length();
		m_clientVerString += wxString::Format(" v0.%u", nClientMinVersion);
		return;
	}


	if (iHashType == SO_OLDEMULE){
		m_clientSoft = SO_OLDEMULE;
		UINT nClientMinVersion = m_nClientVersion;
		m_nClientVersion = MAKE_CLIENT_VERSION(0, nClientMinVersion, 0);
		m_clientVerString = "Old eMule";
		m_SoftLen = m_clientVerString.Length();
		m_clientVerString += wxString::Format(" v0.%u", nClientMinVersion);		
		return;
	}

	m_clientSoft = SO_EDONKEY;
	UINT nClientMinVersion = m_nClientVersion;
	m_nClientVersion = MAKE_CLIENT_VERSION(0, nClientMinVersion, 0);
	m_clientVerString = "eDonkey";
	m_SoftLen = m_clientVerString.Length();
	m_clientVerString += wxString::Format(" v0.%u", nClientMinVersion);
	
}


void CUpDownClient::SetUserName(char* pszNewName)
{
	if (m_pszUsername) {
		delete[] m_pszUsername;
		m_pszUsername = NULL;
	}
	if( pszNewName ) {
		m_pszUsername = nstrdup(pszNewName);
	} 
}

void CUpDownClient::RequestSharedFileList()
{
	if (m_iFileListRequested == 0) {
		theApp.amuledlg->AddDebugLogLine(true,_("Requesting shared files from '%s'"),GetUserName());
		m_iFileListRequested = 1;
		TryToConnect(true);
	} else {
		theApp.amuledlg->AddDebugLogLine(true,_("Requesting shared files from user %s (%u) is already in progress"),GetUserName(),GetUserID());
	}
}

void CUpDownClient::ProcessSharedFileList(char* pachPacket, uint32 nSize, LPCTSTR pszDirectory){
    if (m_iFileListRequested > 0){
        m_iFileListRequested--;
		theApp.searchlist->ProcessSearchanswer(pachPacket,nSize,this,NULL,pszDirectory);
	}
}

void CUpDownClient::ResetFileStatusInfo()
{
	if (m_abyPartStatus) {
		delete[] m_abyPartStatus;
		m_abyPartStatus = NULL;
	}
	m_nPartCount = 0;

	#warning ADDME - Import needed // m_strClientFilename = "";

	m_bCompleteSource = false;
	m_dwLastAskedTime = 0;
	m_iRate=0;
	m_strComment.Empty();
}

wxString CUpDownClient::GetUploadFileInfo()
{
	if(this == NULL) return "";
	wxString sRet;
 
	// build info text and display it
	sRet.Printf(_("NickName: %s\n"), GetUserName(), GetUserID());
	if (reqfile) {
		sRet += _("Requested:") + wxString(reqfile->GetFileName()) + "\n";
		wxString stat;
		stat.Printf(_("Filestats for this session: Accepted %d of %d requests, %s transferred\n")+CString(_("Filestats for all sessions: Accepted %d of %d requests")),
		reqfile->statistic.GetAccepts(), reqfile->statistic.GetRequests(), CastItoXBytes(reqfile->statistic.GetTransfered()).GetData(),
		reqfile->statistic.GetAllTimeAccepts(),
		reqfile->statistic.GetAllTimeRequests(), CastItoXBytes(reqfile->statistic.GetAllTimeTransfered()).GetData() );
		sRet += stat;
	} else {
		sRet += _("Requested unknown file");
	}
	return sRet;
	return "";
}

// Kry - I don't this there's need for this 
#if 0
void CUpDownClient::Destroy()
{
	if (socket) {
		//delete socket;
		socket->Safe_Delete();
		socket = NULL;
	}
}
#endif

// sends a packet, if needed it will establish a connection before
// options used: ignore max connections, control packet, delete packet
// !if the functions returns false it is _possible_ that this clientobject was deleted, because the connectiontry fails 
bool CUpDownClient::SafeSendPacket(Packet* packet)
{
	if (socket && socket->IsConnected()) {
		socket->SendPacket(packet);
		return true;
	} else {
		m_WaitingPackets_list.AddTail(packet);
		return TryToConnect(true);
	}
}

void CUpDownClient::SendPublicKeyPacket(){
	///* delete this line later*/ DEBUG_ONLY(AddDebugLogLine(false, "sending public key to '%s'", GetUserName()));
	// send our public key to the client who requested it
	if (socket == NULL || credits == NULL || m_SecureIdentState != IS_KEYANDSIGNEEDED){
		wxASSERT ( false );
		return;
	}
	if (!theApp.clientcredits->CryptoAvailable())
		return;

	CMemFile data;
	data.Write(theApp.clientcredits->GetPubKeyLen());
	data.WriteRaw(theApp.clientcredits->GetPublicKey(), theApp.clientcredits->GetPubKeyLen());
	Packet* packet = new Packet(&data, OP_EMULEPROT); 
	packet->opcode = OP_PUBLICKEY;
//	Packet* packet = new Packet(OP_PUBLICKEY,theApp.clientcredits->GetPubKeyLen() + 1,OP_EMULEPROT);
//	memcpy(packet->pBuffer+1,theApp.clientcredits->GetPublicKey(), theApp.clientcredits->GetPubKeyLen());
//	packet->pBuffer[0] = theApp.clientcredits->GetPubKeyLen();

	theApp.uploadqueue->AddUpDataOverheadOther(packet->size);
	socket->SendPacket(packet,true,true);
	m_SecureIdentState = IS_SIGNATURENEEDED;
}

void CUpDownClient::SendSignaturePacket(){
	// signate the public key of this client and send it
	if (socket == NULL || credits == NULL || m_SecureIdentState == 0){
		wxASSERT ( false );
		return;
	}

	if (!theApp.clientcredits->CryptoAvailable())
		return;
	if (credits->GetSecIDKeyLen() == 0)
		return; // We don't have his public key yet, will be back here later
		///* delete this line later*/ DEBUG_ONLY(AddDebugLogLine(false, "sending signature key to '%s'", GetUserName()));
	// do we have a challenge value recieved (actually we should if we are in this function)
	if (credits->m_dwCryptRndChallengeFrom == 0){
		theApp.amuledlg->AddDebugLogLine(false, "Want to send signature but challenge value is invalid ('%s')", GetUserName());
		return;
	}
	// v2
	// we will use v1 as default, except if only v2 is supported
	bool bUseV2;
	if ( (m_bySupportSecIdent&1) == 1 )
		bUseV2 = false;
	else
		bUseV2 = true;

	uint8 byChaIPKind = 0;
	uint32 ChallengeIP = 0;
	if (bUseV2){
		if (theApp.serverconnect->GetClientID() == 0 || theApp.serverconnect->IsLowID()){
			// we cannot do not know for sure our public ip, so use the remote clients one
			ChallengeIP = GetIP();
			byChaIPKind = CRYPT_CIP_REMOTECLIENT;
		}
		else{
			ChallengeIP = theApp.serverconnect->GetClientID();
			byChaIPKind  = CRYPT_CIP_LOCALCLIENT;
		}
	}
	//end v2
	uchar achBuffer[250];

	uint8 siglen = theApp.clientcredits->CreateSignature(credits, achBuffer,  250, ENDIAN_SWAP_32(ChallengeIP), byChaIPKind );
	if (siglen == 0){
		wxASSERT ( false );
		return;
	}
	CMemFile data;
	data.Write(siglen);
	data.WriteRaw(achBuffer, siglen);
	if (bUseV2) {
		data.Write(byChaIPKind);
	}	
	Packet* packet = new Packet(&data, OP_EMULEPROT);
	packet->opcode = OP_SIGNATURE;

//	Packet* packet = new Packet(OP_SIGNATURE,siglen + 1+ ( (bUseV2)? 1:0 ),OP_EMULEPROT);
//	memcpy(packet->pBuffer+1,achBuffer, siglen);
//	packet->pBuffer[0] = siglen;
//	if (bUseV2)
//		packet->pBuffer[1+siglen] = byChaIPKind;

	theApp.uploadqueue->AddUpDataOverheadOther(packet->size);
	socket->SendPacket(packet,true,true);
	m_SecureIdentState = IS_ALLREQUESTSSEND;
}

void CUpDownClient::ProcessPublicKeyPacket(uchar* pachPacket, uint32 nSize){
	theApp.clientlist->AddTrackClient(this);

	///* delete this line later*/ DEBUG_ONLY(AddDebugLogLine(false, "recieving public key from '%s'", GetUserName()));
	if (socket == NULL || credits == NULL || pachPacket[0] != nSize-1
		|| nSize == 0 || nSize > 250){
		wxASSERT ( false );
		return;
	}
	if (!theApp.clientcredits->CryptoAvailable())
		return;
	// the function will handle everything (mulitple key etc)
	if (credits->SetSecureIdent(pachPacket+1, pachPacket[0])){
		// if this client wants a signature, now we can send him one
		if (m_SecureIdentState == IS_SIGNATURENEEDED){
			SendSignaturePacket();
		}
		else if(m_SecureIdentState == IS_KEYANDSIGNEEDED){
			// something is wrong
			theApp.amuledlg->AddDebugLogLine(false, "Invalid State error: IS_KEYANDSIGNEEDED in ProcessPublicKeyPacket");
		}
	}
	else{
		theApp.amuledlg->AddDebugLogLine(false, "Failed to use new recieved public key");
	}
}

void CUpDownClient::ProcessSignaturePacket(uchar* pachPacket, uint32 nSize){
	///* delete this line later*/ DEBUG_ONLY(AddDebugLogLine(false, "receiving signature from '%s'", GetUserName()));
	// here we spread the good guys from the bad ones ;)

	if (socket == NULL || credits == NULL || nSize == 0 || nSize > 250){
		wxASSERT ( false );
		return;
	}

	uint8 byChaIPKind;
	if (pachPacket[0] == nSize-1)
		byChaIPKind = 0;
	else if (pachPacket[0] == nSize-2 && (m_bySupportSecIdent & 2) > 0) //v2
		byChaIPKind = pachPacket[nSize-1];
	else{
		wxASSERT ( false );
		return;
	}

	if (!theApp.clientcredits->CryptoAvailable())
		return;
	
	// we accept only one signature per IP, to avoid floods which need a lot cpu time for cryptfunctions
	if (m_dwLastSignatureIP == GetIP()){
		theApp.amuledlg->AddDebugLogLine(false, "recieved multiple signatures from one client");
		return;
	}
	// also make sure this client has a public key
	if (credits->GetSecIDKeyLen() == 0){
		theApp.amuledlg->AddDebugLogLine(false, "recieved signature for client without public key");
		return;
	}
	// and one more check: did we ask for a signature and sent a challange packet?
	if (credits->m_dwCryptRndChallengeFor == 0){
		theApp.amuledlg->AddDebugLogLine(false, "recieved signature for client with invalid challenge value ('%s')", GetUserName());
		return;
	}

	if (theApp.clientcredits->VerifyIdent(credits, pachPacket+1, pachPacket[0], ENDIAN_SWAP_32(GetIP()), byChaIPKind ) ){
		// result is saved in function abouve
		//AddDebugLogLine(false, "'%s' has passed the secure identification, V2 State: %i", GetUserName(), byChaIPKind);
	}
	else {
		theApp.amuledlg->AddDebugLogLine(false, "'%s' has failed the secure identification, V2 State: %i", GetUserName(), byChaIPKind);
	}
	m_dwLastSignatureIP = GetIP(); 
}

void CUpDownClient::SendSecIdentStatePacket(){
	// check if we need public key and signature
	uint8 nValue = 0;
	if (credits){
		if (theApp.clientcredits->CryptoAvailable()){
			if (credits->GetSecIDKeyLen() == 0) {
				nValue = IS_KEYANDSIGNEEDED;
			} else if (m_dwLastSignatureIP != GetIP()) {
				nValue = IS_SIGNATURENEEDED;
			}
		}
		if (nValue == 0){
			theApp.amuledlg->AddDebugLogLine(false, "Not sending SecIdentState Packet, because State is Zero");
			return;
		}
		// crypt: send random data to sign
		uint32 dwRandom = rand()+1;
		credits->m_dwCryptRndChallengeFor = dwRandom;
		// Kry - Too much output, it already works.
		//theApp.amuledlg->AddDebugLogLine(false, "sending SecIdentState Packet, state: %i (to '%s')", nValue, GetUserName() );

		CMemFile data;
		data.Write(nValue);
		data.Write(dwRandom);
		Packet* packet = new Packet(&data, OP_EMULEPROT);
		packet->opcode = OP_SECIDENTSTATE;		
//		Packet* packet = new Packet(OP_SECIDENTSTATE,5,OP_EMULEPROT);
//		packet->pBuffer[0] = nValue;
//		memcpy(packet->pBuffer+1,&dwRandom, sizeof(dwRandom));

		theApp.uploadqueue->AddUpDataOverheadOther(packet->size);
		socket->SendPacket(packet,true,true);
	}
	else {
		wxASSERT ( false );
	}
}

void CUpDownClient::ProcessSecIdentStatePacket(uchar* pachPacket, uint32 nSize){
	if (nSize != 5)
		return;
	if (!credits){
		wxASSERT ( false );
		return;
	}
	switch(pachPacket[0]){
			case 0:
				m_SecureIdentState = IS_UNAVAILABLE;
				break;
			case 1:
				m_SecureIdentState = IS_SIGNATURENEEDED;
				break;
			case 2:
				m_SecureIdentState = IS_KEYANDSIGNEEDED;
				break;
		}
	CSafeMemFile data((BYTE*)pachPacket,nSize);
	// Kry:  + 1 on the original one.
	try {
		byte discard;
		data.Read(discard);		
		uint32 dwRandom;
		data.Read(dwRandom);
		credits->m_dwCryptRndChallengeFrom = dwRandom;
	} 
	catch ( CStrangePacket )
	{
		printf("\nWrong Tags on SecIdentState packet!!\n");
		printf("Sent by %s on ip %s port %i using client %i version %i\n",GetUserName(),GetFullIP(),GetUserPort(),GetClientSoft(),GetMuleVersion());
		printf("User Disconnected.\n");
		throw wxString("Wrong Tags on SecIdentState packet");
	}
	catch ( CInvalidPacket (e))
	{
		printf("Wrong Tags on SecIdentState packet - %s\n\n",e.what());
		printf("Sent by %s on ip %s port %i using client %i version %i\n",GetUserName(),GetFullIP(),GetUserPort(),GetClientSoft(),GetMuleVersion());
		printf("User Disconnected.\n");		
		throw wxString("Wrong Tags on SecIdentState packet");
	}
			
	
	//DEBUG_ONLY(AddDebugLogLine(false, "recieved SecIdentState Packet, state: %i", pachPacket[0]));
}


void CUpDownClient::InfoPacketsReceived(){
	// indicates that both Information Packets has been received
	// needed for actions, which process data from both packets
	wxASSERT ( m_byInfopacketsReceived == IP_BOTH );
	m_byInfopacketsReceived = IP_NONE;
	
	if (m_bySupportSecIdent){
		SendSecIdentStatePacket();
	}
}

bool CUpDownClient::CheckHandshakeFinished(UINT protocol, UINT opcode) const
{
	if (m_bHelloAnswerPending){
		//throw CString(_T("Handshake not finished")); // -> disconnect client
		// this triggers way too often.. need more time to look at this -> only create a warning
		if (theApp.glob_prefs->GetVerbose()) {
			theApp.amuledlg->AddLogLine(false, _("Handshake not finished while processing packet."));
		}
		return false;
	}

	return true;
}

void CUpDownClient::CheckForGPLEvilDoer(){
	// check for known major gpl breaker 
	if (m_strModVersion.Trim().MakeUpper().Find("LH") == 0 || m_strModVersion.Trim().MakeUpper().Find("LIO") == 0 || m_strModVersion.Trim().MakeUpper().Find("LI0") == 0){
		m_bGPLEvildoer = true;
	}
}

wxString CUpDownClient::GetClientFullInfo() {

	if (m_clientVerString.IsEmpty()) {
		ReGetClientSoft();		
	}
	
	wxString FullVerName;
	FullVerName = "Client ";
	if (!m_pszUsername) {
		FullVerName += "(Unknown)";
	} else {
		FullVerName += m_pszUsername;
	}
	FullVerName += wxString::Format(" on ip %s port %u using ",GetFullIP(),GetUserPort()) + m_clientVerString;
	if (!GetClientModString().IsEmpty()) {		
		FullVerName += " Mod " + GetClientModString();
	}
	return (FullVerName);
}
