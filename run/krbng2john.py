#!/usr/bin/env python2

# http://anonsvn.wireshark.org/wireshark/trunk/doc/README.xml-output
# tshark -r AD-capture-2.pcapng -T pdml  > ~/data.pdml
# krbng2john data.pdml

import sys
try:
    from lxml import etree
except ImportError:
    print >> sys.stderr, "This program needs lxml libraries to run!"
    sys.exit(1)
import binascii

def process_file(f):

    xmlData = etree.parse(f)

    messages = [e for e in xmlData.xpath('/pdml/packet/proto[@name="kerberos"]')]

    state = None
    encrypted_timestamp = None
    server = None

    for msg in messages:
        if msg.attrib['showname'] == "Kerberos AS-REQ":
            if not state:
                state = "AS-REQ"
            else:
                state = "AS-REQ2"
                # actual request with encrypted timestamp
                fields = msg.xpath(".//field")
                for field in fields:
                    if 'name' in field.attrib:
                        if field.attrib['name'] == 'kerberos.PA_ENC_TIMESTAMP.encrypted':
                            encrypted_timestamp = field.attrib['value']
        if msg.attrib['showname'] == "Kerberos KRB-ERROR":
            if state == "AS-REQ":
                state = "KRB-ERROR"
            else:
                print "unkwown"
            # note down the salt
            fields = msg.xpath(".//field")
            for field in fields:
                if 'name' in field.attrib:
                    if field.attrib['name'] == 'kerberos.etype_info2.salt':
                        salt = field.attrib["value"]
                        server = "AD"
                    if field.attrib['name'] == 'kerberos.realm':
                        realm = field.attrib['show']
                        server = "plain"
                    if field.attrib['name'] == 'kerberos.cname':
                        user = field.attrib['showname'][25:]

        if msg.attrib['showname'] == "Kerberos AS-REP":
            if state == "AS-REQ2":
                if server == "AD":
                    print "%s:$krb5ng$1$%s$%s$%s " % (binascii.unhexlify(salt), binascii.unhexlify(salt), encrypted_timestamp[0:88], encrypted_timestamp[88:])
                else:
                    print "%s:$krb5ng$0$%s$%s$%s$%s " % (user, user, realm, encrypted_timestamp[0:88], encrypted_timestamp[88:])

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print "Usage: %s <pdml file(s)>" % sys.argv[0]
        sys.exit(1)

    for i in range(1, len(sys.argv)):
        process_file(sys.argv[i])

