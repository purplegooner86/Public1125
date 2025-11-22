import struct
from collections import OrderedDict

from socket import inet_ntop,AF_INET,AF_INET6

import argparse
import sys, datetime, json

from hashlib import md5

class MTConfig(object):
	
	def __init__(self, datFileName, idxFileName=None):
		self.__db = open(datFileName, 'rb')
		if idxFileName is None:
			self.__index = None
		else:
			self.__index = open(idxFileName, 'rb')
		self.__ip = 0
		self.mapping = {0xfe0009 : 'comment',
						0xfe0001: 'record_id',
						0xfe000a: 'disabled',
						0xfe000d: 'default',
						0xffffff: 'index_id',
						0xfe0008: 'active',
						0xfe0010: 'name',
						}
		self.decode = False
		self.returnTypes = False
		self.preserveOrder = False
		self.parsers = {}
		self.filters = {}
		self.__idFormat = "_%c%x"
		self.__idFormatTypeLocation = 1
	
	@property
	def idFormat(self):
		return self.__idFormat
		
	@idFormat.setter
	def idFormat(self, idFormat):
		
		for i in map(chr, range(33, 48)):
			if i not in idFormat:
				break
		if i == "0":
			raise Exception("Unable to detect type position inside id format. Try using less characters.")
		
		self.__idFormat = idFormat
		
		test = idFormat %(i,0xAABBCC)
		if i in test:
			self.__idFormatTypeLocation = test.index(i)
		else:
			self.__idFormatTypeLocation = None

			
	def __iter__(self):
		return self


	def __next__(self):
		
		if self.__index is None:
			try:
				size, = struct.unpack("<H",self.__db.read(2))
				parsed = self.parse_record(self.__db.read(size-2))
				if parsed is not None and self.returnTypes:
					parsed[iid_r] = (0x08,parsed[iid_r])
				return parsed
			except:
				print("raising StopIteration")
				raise StopIteration
			
		iid = 0xFFFFFFFF
		while iid == 0xFFFFFFFF:
			ientry = self.__index.read(12)
			if len(ientry)<12:
				raise StopIteration
			iid, = struct.unpack("<I",ientry[0:4])
			ilen, = struct.unpack("<I",ientry[4:8])
			isep, = struct.unpack("<I",ientry[8:12])
			self.__ip += ilen
		
		if isep != 5:
			print("Non-standard index separator %08X." % isep)
		
		self.__db.seek(self.__ip - ilen)
		size, = struct.unpack("<H",self.__db.read(2))
		parsed = self.parse_record(self.__db.read(size-2))
		
		if parsed is None:
			print("parsed is none")
			return None
		
		iid_r = self.__idFormat % ("$",0xffffff)
		if self.decode:
			if (0xffffff) in self.mapping:
				iid_r = self.mapping[0xffffff]
			elif iid_r in self.mapping:
				iid_r = self.mapping[iid_r]
				
		parsed[iid_r] = self.parse_value(0xffffff,iid_r,0x08,iid)

		if self.returnTypes:
			parsed[iid_r] = (0x08,parsed[iid_r])
			
		return parsed
		
	def __mangle_data(self,what,which,how):
		if which in how:
			if isinstance(what,list):
				what_r=[ how[which](x) for x in what ] 
				go = sum([ x is not None for x in what_r ])
				if go == len(what):
					return what_r
				
			else:
				what_r=how[which](what)
				if what_r is not None:
					return what_r
					
		return None
		
	def parse_value(self,blockid_raw,blockid,blocktype,data):
		data_r = self.__mangle_data(data,blockid,self.parsers)
		if data_r is not None:
			return data_r
		data_r = self.__mangle_data(data,blockid_raw,self.parsers)
		if data_r is not None:
			return data_r
		data_r = self.__mangle_data(data,blocktype,self.filters)
		if data_r is not None:
			return data_r
		if self.__idFormatTypeLocation is not None:
			data_r = self.__mangle_data(data,blockid[self.__idFormatTypeLocation],self.filters)
			if data_r is not None:
				return data_r		
		data_r = self.__mangle_data(data,type(data),self.filters)
		if data_r is not None:
			return data_r
		return data
					
	
	def parse_record(self, record, topID=0):
		if self.preserveOrder:
			alldata = OrderedDict()
		else:
			alldata = {}

		if record[0:2] != b'\x4D\x32':
			print("Not MT DAT record.")
			return None
		
		bpointer = 2
		while bpointer+4 < len(record):
			bmarker, = struct.unpack("<I",record[bpointer:bpointer+4])
			bpointer += 4
			bidraw = bmarker & 0xffffff
					
			btype = bmarker >> 24
			
			blen = None
			data = None
			

			'''
			btype ........
			      0000000, - boolean 
			      ,,1,1,,, - M2 block (len = short)
			      ,,11,,,, - binary data block (len = short)
			      ,,,,,,,1 - one byte
			      ,,,,,,1, - ???
			      ,,,,,1,, - ???
			      ,,,11,,, - 128bit int
			      ,,,,1,,, - int (four bytes)
			      ,,,1,,,, - long (8 bytes)
			      ,,1,,,,, - string
			      ,1,,,,,, - ??? unused? or long array of?
			      1,,,,,,, - short array of
			      			      
			types (MT notation)
				(CAPITAL X = list of x)

				a,A, (0x18) IPv6 address (or duid)
				b,B, bool
				  M, multi
				q,Q, (0x10) big number
				r,R, (0x31) mac address
				s,S, (0x21) string
				u,U, unsigned integer

			'''
			if btype == 0x21: #freelength short string	
				blen = record[bpointer]
				bpointer += 1
				data = record[bpointer:bpointer+blen]
				mtype = "s"
			elif btype == 0x31: #freelength list of bytes (mac address)
				blen = record[bpointer]
				bpointer += 1
				mtype = "r"
				data = [e for e in record[bpointer:bpointer+blen]]
			elif btype == 0x08: #int
				blen = 4
				data, = struct.unpack("<I",record[bpointer:bpointer+blen])
				mtype = "u"
			elif btype == 0x10: #long
				blen = 8
				data, = struct.unpack("<Q",record[bpointer:bpointer+blen])
				mtype = "q"
			elif btype == 0x18: #128bit integer
				blen = 16
				data = [e for e in record[bpointer:bpointer+blen]]
				mtype = "a"
			elif btype == 0x09: # byte
				blen = 1
				data, = struct.unpack("B",record[bpointer:bpointer+blen])
				mtype = "u"
			elif btype == 0x29: # single short M2 block
				blen = 1
				sub_size, = struct.unpack("B",record[bpointer:bpointer+blen])
				data = self.parse_record(record[bpointer+1:bpointer+1+sub_size],topID=(topID<<24)+bidraw)
				bpointer += sub_size
				mtype = "M"
			elif btype == 0xA8: # array of M2 blocks
				blen = 2
				arraysize, = struct.unpack("<H",record[bpointer:bpointer+blen])
				parser = 0
				data = []
				while parser < arraysize:
					parser += 1
					bpointer += blen
					sub_size, = struct.unpack("<H",record[bpointer:bpointer+2])
					bpointer += 2
					data.append(self.parse_record(record[bpointer:bpointer+sub_size],topID=(topID<<24)+bidraw))
					# MT has a bug here ^^, replicate it 
					
					bpointer += sub_size - 2
				mtype = "M"
			elif btype == 0x88: #array of int
				blen = 2
				arraysize, = struct.unpack("<H",record[bpointer:bpointer+blen])
				parser = 0
				data = []
				while parser < arraysize:
					parser += 1
					data.append(struct.unpack("<I",record[bpointer+blen:bpointer+blen+4])[0])
					bpointer += 4
				mtype = "U"
			elif btype == 0xA0: #array of strings
				blen = 2
				arraysize, = struct.unpack("<H",record[bpointer:bpointer+blen])
				parser = 0
				data = []
				while parser < arraysize:
					parser += 1
					bpointer += blen
					sub_size, = struct.unpack("<H",record[bpointer:bpointer+2])
					bpointer += 2
					data.append(record[bpointer:bpointer+sub_size])
					bpointer += sub_size - 2
				mtype = "S"
				
			elif btype == 0x00 or btype == 0x01:  # boolean
				blen = 0
				data = False if not btype else True
				mtype = "b"
			else:
				print("Unknown datatype %02X." % btype)
				mtype = " "
				blen = 0
				data = None
			
			bid = self.__idFormat % (mtype,bidraw)
			if self.decode:
				if ((topID<<24) + bidraw) in self.mapping:
					bid = self.mapping[((topID<<24) + bidraw)]
				elif bid in self.mapping:
					bid = self.mapping[bid]


			data = self.parse_value	((topID<<24) + bidraw,bid,btype,data)
			
			
			if bid in alldata:
				print("Record contains multiple blocks %06X that translate to the same ID (\"%s\"), ignoring extra blocks. Check your mapping and idFormat." % (bidraw, bid))
			else:
				if self.returnTypes:
					alldata[bid] = (btype,data)
				else:
					alldata[bid] = data
			
			bpointer += blen

		return alldata
		
		
		
	def mapBlockNames(self, mapping):
		self.mapping.update(mapping)
		self.decode = True

	def addParser(self, blockid, function):
		self.parsers[blockid] = function
		
	def addFilter(self, blocktype, function):
		self.filters[blocktype] = function


def xor(data, key): 
    return str(bytearray(a^b for a, b in zip(*map(bytearray, [data, key])))).split("\x00",2)[0]

def parseIPv4(data):
	return inet_ntop(AF_INET, struct.pack("<L", data))

def parseIPv6(data):
	return inet_ntop(AF_INET6,''.join(map(chr,data)))

def decToHex(data):
	if type(data) != int:
		print("Cannot convert non int to hex!")
		return str(data)
	return f"{data:2x}"
	
def parseAddressNet(data):
	ip = None
	netmask = None
	
	for blocktype in data:
		if blocktype == "_u1" or blocktype == "_u5" :
			ip = parseIPv4(data[blocktype])
		elif blocktype =="_a3":
			ip = parseIPv6(data[blocktype])
		elif blocktype =="_u4":
			netmask = str(data[blocktype])
		elif blocktype == "_u2" or blocktype == "_u6" :
			netmask = str(bin(data[blocktype]).count("1"))
			
	if ip and netmask:
		return ip+"/"+netmask


def parseMTdate(data):
	return datetime.datetime.fromtimestamp(data).strftime('%b/%d/%Y %H:%M:%S')

def parseMTusergroup(data):
	# this may be incorrect. depends on group.dat ;)
	groups = ["read","write","full"]
	if data>=0 and data<len(groups):
		return groups[data]


def do_user(directory, summary):
	database="user"

	conf = MTConfig(directory+"/"+database+".dat",directory+"/"+database+".idx")

	conf.mapBlockNames( {0xb:"permissions", 0x1f:"last_login", 0x1c:"password_set",
						0x1:"username",0x11:"password",0x2:"group",0x12:"groupname",
						0x10:"allowed_addresses",0x5:"allowed_ip4", 0x6:"allowed_net4" })

	conf.addParser (0x1f, parseMTdate)
	conf.addParser (0x12, parseMTusergroup)
	conf.addParser (0xb, lambda x: "%x" % x)
	conf.addParser (0x5, parseIPv4)
	conf.addParser (0x6, parseIPv4)
	conf.addParser (0x10, parseAddressNet)

	ret_string = ""

	for record in conf:
		if "username" in record and "password" in record: # http://hop.02.lv/2Wb
			record["#key"]=md5(record["username"]+b'283i4jfkai3389').digest()	
			record["password"]=xor(record["password"],record["#key"]*16)
			record["#key"]=" ".join("{:02x}".format(c) for c in record["#key"])

		if summary:
			ret_string += f" ---- User ----\n"
			ret_string += f"username: {record.get('username')}\n"
			ret_string += f"password: {record.get('password')}\n"
			ret_string += f"last login: {record.get('last_login')}\n"
		else:
			ret_string += str(record) + "\n"

	return ret_string

def do_ovpn(directory, summary):
	database = "ovpn/server"

	conf = MTConfig(directory+"/"+database+".dat",directory+"/"+database+".idx")

	conf.mapBlockNames({
		0xc8:"Enabled?",
		0xd2:"Require Client Cert?",
		0xcf:"MAC Address",
		0xc9:"Port"
	})

	conf.addParser(0xcf, decToHex)

	ret_string = ""

	for record in conf:
		if summary:
			ret_string += f" ---- OVPN Server config ----\n"
			ret_string += f"Enabled? {record.get('Enabled?')}\n"
			ret_string += f"Port: {record.get('Port')}\n"
			ret_string += f"MAC Address: {record.get('MAC Address')}\n"
			ret_string += f"Require Client Cert? {record.get('Require Client Cert?')}\n"
		else:
			ret_string += str(record) + "\n"
	
	return ret_string


def do_radius(directory, summary):
	# u4 is radius server IP, u6 is authentication port, u7 is accounting port, s5 is Secret
	# fe000a is disabled
	# fe0009 is a comment
	# u1 is the flags:
	#	0b1 is PPP
	# 	0b100000000 is Login
	#	0b1000000000 is hotspot
	#	0b10000000000 is wireless
	#	0b1000000000000 is DHCP
	#	0b10000000000000 is IPSec
	#	0b100000000000000 is Dot1x

	database = "radius"

	conf = MTConfig(directory+"/"+database+".dat",directory+"/"+database+".idx")

	conf.mapBlockNames({
		0x04: "Radius Server IP",
		0x06: "Authentication Port",
		0x07: "Accounting Port",
		0x05: "Secret",
		0xfe000a: "Disabled?",
		0xfe0009: "Comment",
		0x01: "ServiceFlags"
	})

	conf.addParser(0x04, parseIPv4)

	ret_string = ""

	for record in conf:
		if summary:
			used_for_services = []
			service_flags = record.get('ServiceFlags')
			if service_flags:
				if service_flags & 0b1:
					used_for_services.append("PPP")
				if service_flags & 0b100000000:
					used_for_services.append("Login")
				if service_flags & 0b1000000000:
					used_for_services.append("HotSpot")
				if service_flags & 0b10000000000:
					used_for_services.append("Wireless")
				if service_flags & 0b1000000000000:
					used_for_services.append("DHCP")
				if service_flags & 0b10000000000000:
					used_for_services.append("IPSec")
				if service_flags & 0b100000000000000:
					used_for_services.append("Dot1x")

			ret_string += " ----- RADIUS Entry ------\n"
			ret_string += f"Radius Server IP: {record.get('Radius Server IP')}\n"
			ret_string += f"Authentication Port: {record.get('Authentication Port')}\n"
			ret_string += f"Accounting Port: {record.get('Accounting Port')}\n"
			ret_string += f"Secret: {record.get('Secret')}\n"
			ret_string += f"Disabled? {record.get('Disabled?')}\n"
			ret_string += f"Comment: {record.get('Comment')}\n"
			ret_string += f"Used for Services:"
			for e in used_for_services:
				ret_string += f" {e},"
			ret_string += "\n"
		
		else:
			ret_string += str(record) + "\n"

	return ret_string


def do_devices(directory, summary):
	# s10006 is interface name
	# s10030 and s10009 are both MAC addresses
	# fe0001 is the interface id
	# There is more to parse here, but I don't know how
	# If its a ethernet, 10001 seems to be 1 and 10002 seems to be 270959
	# If its a vlan, 10001 seems to be 30 and 10002 seems to be 8815, and sub_list[7d2] seems to be parent
	# If its a bridge, 10001 seems to be 12 and 10002 seems to be 98863. sub_list[3e9] is the MAC address.
	# If the bridge is associated with a ethernet interface, it will have the same MAC address as that interface
	# If its a gre tunnel, 10001 seems to be 55 and 10002 seems to be 12

	database = "net/devices"

	conf = MTConfig(directory+"/"+database+".dat",directory+"/"+database+".idx")

	conf.mapBlockNames({
		0x10006: "Interface Name",
		0x10030: "MAC Address 1",
		0x10009: "MAC Address 2",
		0xfe0001: "Interface Id",
		0x10008: "SubList"
	})

	ret_string = ""

	for record in conf:
		if summary:
			ret_string += " ---- Device ----\n"
			ret_string += f"Interface Name: {record.get('Interface Name')}\n"
			ret_string += f"Interface Id: {record.get('Interface Id')}\n"
			ret_string += f"MAC Addr 1: {record.get('MAC Address 1')}\n"
			ret_string += f"MAC Addr 2: {record.get('MAC Address 2')}\n"
			sub_list = record.get("SubList")
			if sub_list:
				print(sub_list)
				ret_string += f"Maybe Parent Interface id: {sub_list.get('_u7d2')}\n"
		else:
			ret_string += str(record) + "\n"

	return ret_string

def do_addrs(directory, summary):
	# u1 is address
	# u2 is network
	# u3 is subnet mask
	# u5, ua or fe0001 specifies the interface index
	# fe0009 is comment

	database = "net/addrs"

	conf = MTConfig(directory+"/"+database+".dat",directory+"/"+database+".idx")

	conf.mapBlockNames({
		0x01:"Address",
		0x02:"Network",
		0x03:"Subnet Mask",
		0x05:"Interface Id 1",
		0x0a:"Interface Id 2",
		0xfe0001:"Interface Id 3",
		0xfe0009:"Comment"
	})

	conf.addParser(0x01, parseIPv4)
	conf.addParser(0x02, parseIPv4)
	conf.addParser(0x03, parseIPv4)

	ret_string = ""

	for record in conf:
		if summary:
			ret_string += " ---- IP Address ---- \n"
			ret_string += f"Address: {record.get('Address')}\n"
			ret_string += f"Network: {record.get('Network')}\n"
			ret_string += f"Subnet Mask: {record.get('Subnet Mask')}\n"
			ret_string += f"Interface Ids? {record.get('Interface Id 1')} {record.get('Interface Id 2')} {record.get('Interface Id 3')}\n"
			ret_string += f"Comment: {record.get('Comment')}\n"
			ret_string += f"Disabled? {record.get('disabled')}\n"
		else:
			ret_string += str(record) + "\n"
		
	return ret_string


def do_firewall(directory, summary):
	# u62 is a list of src port
	# u63 is a list of dest port
	# ub is protocol: 6=tcp, 17=udp, 47=gre, 1=icmp, 2=igmp, 27=rdp
	# s27 is chain
	# fe0009 is a comment
	# u34 and u35 are dest addresses
	# u32 and u33 are source addresses
	# u28 is action: 0=accept, 10=drop, 11=reject

	# aff0013 shows you the IP address where the firewall rule was created from
	

	action_type = {
		0: "accept",
		10: "drop",
		11: "reject"
	}

	protocols = {
		6: "tcp",
		17: "udp",
		47: "gre",
		1: "icmp",
		2: "igmp",
		27: "rdp"
	}

	database = "net/ipt-filter"

	conf = MTConfig(directory+"/"+database+".dat",directory+"/"+database+".idx")

	conf.mapBlockNames({
		0x62: "Source Ports",
		0x63: "Dst Ports",
		0x0b: "Protocol",
		0x27: "chain",
		0x32: "Source Address 1",
		0x33: "Source Address 2",
		0x34: "Dest Address 1",
		0x35: "Dest Address 2",
		0x28: "action",
		0xfe000a: "Disabled?",
		0xfe0009: "Comment",
		0xff0013: "Rule Created From" 
	})

	conf.addParser(0x32, parseIPv4)
	conf.addParser(0x33, parseIPv4)
	conf.addParser(0x34, parseIPv4)
	conf.addParser(0x35, parseIPv4)

	conf.addParser(0x28, lambda x: action_type.get(x))
	conf.addParser(0x0b, lambda x: protocols.get(x))

	ret_string = ""

	for record in conf:
		if summary:
			ret_string += "---- Firewall Rule ----\n"
			ret_string += f"Source Ports: {record.get('Source Ports')}\n"
			ret_string += f"Dst Ports: {record.get('Dst Ports')}\n"
			ret_string += f"Protocol: {record.get('Protocol')}\n"
			ret_string += f"chain: {record.get('chain')}\n"
			ret_string += f"Source Address 1: {record.get('Source Address 1')}\n"
			ret_string += f"Source Address 2: {record.get('Source Address 2')}\n"
			ret_string += f"Dest Address 1: {record.get('Dest Address 1')}\n"
			ret_string += f"Dest Address 2: {record.get('Dest Address 2')}\n"
			ret_string += f"Action: {record.get('action')}\n"
			ret_string += f"Disabled? {record.get('Disabled?')}\n"
			ret_string += f"Rule Created From: {record.get('Rule Created From')}\n"
			ret_string += f"Comment: {record.get('Comment')}\n"
		else:
			ret_string += str(record) + "\n"

	return ret_string
	

def do_secrets(directory, summary):
	# u3 is service type
	# any: 0 async: 1 PPTP: 2 PPOE: 3 L2TP: 5 ovpn: 6 SSTP: 7
	service_types = {
		0: "all",
		1: "async",
		2: "pptp",
		3: "ppoe",
		5: "l2tp",
		6: "ovpn",
		7: "sstp"
	}

	database = "ppp/secret"

	conf = MTConfig(directory+"/"+database+".dat",directory+"/"+database+".idx")

	conf.mapBlockNames({
		0x01:"Username",
		0x02:"Password",
		0x03:"Service Type",
		0x0d:"Last Logged Out",
		0x0f:"Last Caller Id"
	})

	conf.addParser(0x03, lambda x: service_types.get(x))
	conf.addParser(0x0d, parseMTdate)

	ret_string = ""

	for record in conf:
		if summary:
			ret_string += " ---- Secret Profile ---- \n"
			ret_string += f"Username: {record.get('Username')}\n"
			ret_string += f"Password: {record.get('Password')}\n"
			ret_string += f"Service Type: {record.get('Service Type')}\n"
			ret_string += f"Last Logged Out: {record.get('Last Logged Out')}\n"
			ret_string += f"Last Caller Id: {record.get('Last Caller Id')}\n"
		else:
			ret_string += str(record) + "\n"


	return ret_string


def do_target_file(directory, target_file):
	conf = MTConfig(directory + "/" + target_file + ".dat", directory + "/" + target_file + ".idx")
	conf.returnTypes = True
	conf.preserveOrder = True

	for record in conf:
		print("Start of record")
		
		for block in record:
			(btype,data) = record[block]
			print(f"({btype:2x}) {block} {type(data)} {len(data) if isinstance(data,(str,list,dict)) else ''} {'' if (btype & 0x28 == 0x28) else data}")
			if btype & 0x28 == 0x28:
				if btype & 0x80 != 0x80:
					data=[data]
				
				for item in data:
					if btype & 0x80 == 0x80:
						print(f"   -> ({(btype & ~0x80):2x}) {block} {type(data)} {len(item)}")
					for block_s in item:
						(btype_s,data_s) = item[block_s]
						print("   "*(2 if (btype & 0x80 == 0x80) else 1)+"-> (%02X) %s %s %s%s" % (btype_s, block_s, type(data_s),"{%i} "%len(data_s) if isinstance(data_s,(str,list,dict)) else "", data_s))
					

		print("End of record")


def parse_args() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser()
	subparsers = parser.add_subparsers(dest='function')

	secrets_parser = subparsers.add_parser("secrets")
	secrets_parser.add_argument("-d", "--directory", required=True, help="directory where the .dat files are")
	secrets_parser.add_argument("--summary", action="store_true", help="summary mode? or not summary mode")

	user_parser = subparsers.add_parser("user")
	user_parser.add_argument("--summary", action="store_true", help="summary mode? or not summary mode")
	user_parser.add_argument("-d", "--directory", required=True, help="directory where the .dat files are")


	target_file_parser = subparsers.add_parser("target_file")
	target_file_parser.add_argument("-d", "--directory", required=True, help="directory where the .dat files are")
	target_file_parser.add_argument("-t", "--target", required=True, help="path to file you are interested in")

	ovpn_parser = subparsers.add_parser("ovpn")
	ovpn_parser.add_argument("--summary", action="store_true", help="summary mode? or not summary mode")
	ovpn_parser.add_argument("-d", "--directory", required=True, help="directory where the .dat files are")

	addrs_parser = subparsers.add_parser("addrs")
	addrs_parser.add_argument("--summary", action="store_true", help="summary mode? or not summary mode")
	addrs_parser.add_argument("-d", "--directory", required=True, help="directory where the .dat files are")
	
	devices_parser = subparsers.add_parser("devices")
	devices_parser.add_argument("--summary", action="store_true", help="summary mode? or not summary mode")
	devices_parser.add_argument("-d", "--directory", required=True, help="directory where the .dat files are")

	radius_parser = subparsers.add_parser("radius")
	radius_parser.add_argument("--summary", action="store_true", help="summary mode? or not summary mode")
	radius_parser.add_argument("-d", "--directory", required=True, help="directory where the .dat files are")

	firewall_parser = subparsers.add_parser("firewall")
	firewall_parser.add_argument("--summary", action="store_true", help="summary mode? or not summary mode")
	firewall_parser.add_argument("-d", "--directory", required=True, help="directory where the .dat files are")

	args = parser.parse_args()
	
	return args


if __name__ == '__main__':

	args = parse_args()

	if args.function == "user":
		print(do_user(args.directory, args.summary))
	elif args.function == "secrets":
		print(do_secrets(args.directory, args.summary))
	elif args.function == "target_file":
		do_target_file(args.directory, args.target)
	elif args.function == "ovpn":
		print(do_ovpn(args.directory, args.summary))
	elif args.function == "addrs":
		print(do_addrs(args.directory, args.summary))
	elif args.function == "devices":
		print(do_devices(args.directory, args.summary))
	elif args.function == "radius":
		print(do_radius(args.directory, args.summary))
	elif args.function == "firewall":
		print(do_firewall(args.directory, args.summary))
	else:
		print(f"unsuported function: {args.function}")
	