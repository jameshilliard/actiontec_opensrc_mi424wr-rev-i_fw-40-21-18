// Copyright (c) 1999-2004 Brian Wellington (bwelling@xbill.org)

package org.xbill.DNS;

import java.io.*;
import java.util.*;

/**
 * A class that tries to locate name servers and the search path to
 * be appended to unqualified names.  Currently, this works if either the
 * appropriate properties are set, the OS has a unix-like /etc/resolv.conf,
 * or the system is Windows based with ipconfig or winipcfg.  These routines
 * will be called internally by the Lookup class, and can also be called
 * directly if desired.
 *
 * @author Brian Wellington
 */

public class ResolverConfig {

private static String [] servers = null;
private static Name [] searchlist = null;

private static ResolverConfig currentConfig;

static {
	refresh();
}

public
ResolverConfig() {
	findProperty();
	if (servers == null || searchlist == null) {
		String OS = System.getProperty("os.name");
		if (OS.indexOf("Windows") != -1) {
			if (OS.indexOf("95") != -1 ||
			    OS.indexOf("98") != -1 ||
			    OS.indexOf("ME") != -1)
				find95();
			else
				findNT();
		} else if (OS.indexOf("NetWare") != -1)
			findNetware();
		else
			findUnix();
	}
}

private void
addServer(String server, List list) {
	if (list.contains(server))
		return;
	if (Options.check("verbose"))
		System.out.println("adding server " + server);
	list.add(server);
}

private void
addSearch(String search, List list) {
	Name name;
	if (Options.check("verbose"))
		System.out.println("adding search " + search);
	try {
		name = Name.fromString(search, Name.root);
	}
	catch (TextParseException e) {
		return;
	}
	if (list.contains(name))
		return;
	list.add(name);
}

/**
 * Looks in the system properties to find servers and a search path.
 * Servers are defined by dns.server=server1,server2...
 * The search path is defined by dns.search=domain1,domain2...
 */
private void
findProperty() {
	String s, prop;
	List l = new ArrayList(0);
	StringTokenizer st;

	prop = System.getProperty("dns.server");
	if (prop != null) {
		st = new StringTokenizer(prop, ",");
		while (st.hasMoreTokens())
			addServer(st.nextToken(), l);
		if (l.size() > 0)
			servers = (String []) l.toArray(new String[l.size()]);
	}

	l.clear();
	prop = System.getProperty("dns.search");
	if (prop != null) {
		st = new StringTokenizer(prop, ",");
		while (st.hasMoreTokens()) {
			addSearch(st.nextToken(), l);
		}
		if (l.size() > 0)
			searchlist = (Name []) l.toArray(new Name[l.size()]);
	}
}

/**
 * Looks in /etc/resolv.conf to find servers and a search path.
 * "nameserver" lines specify servers.  "domain" and "search" lines
 * define the search path.
 */
private void
findResolvConf(String file) {
	InputStream in = null;
	try {
		in = new FileInputStream(file);
	}
	catch (FileNotFoundException e) {
		return;
	}
	InputStreamReader isr = new InputStreamReader(in);
	BufferedReader br = new BufferedReader(isr);
	List lserver = new ArrayList(0);
	List lsearch = new ArrayList(0);
	try {
		String line;
		while ((line = br.readLine()) != null) {
			if (line.startsWith("nameserver")) {
				StringTokenizer st = new StringTokenizer(line);
				st.nextToken(); /* skip nameserver */
				addServer(st.nextToken(), lserver);
			}
			else if (line.startsWith("domain")) {
				StringTokenizer st = new StringTokenizer(line);
				st.nextToken(); /* skip domain */
				if (!st.hasMoreTokens())
					continue;
				addSearch(st.nextToken(), lsearch);
			}
			else if (line.startsWith("search")) {
				StringTokenizer st = new StringTokenizer(line);
				st.nextToken(); /* skip search */
				String s;
				while (st.hasMoreTokens())
					addSearch(st.nextToken(), lsearch);
			}
		}
		br.close();
	}
	catch (IOException e) {
	}

	if (servers == null && lserver.size() > 0)
		servers =
			(String [])lserver.toArray(new String[lserver.size()]);

	if (searchlist == null && lsearch.size() > 0)
		searchlist =
			(Name [])lsearch.toArray(new Name[lsearch.size()]);
}

private void
findUnix() {
	findResolvConf("/etc/resolv.conf");
}

private void
findNetware() {
	findResolvConf("sys:/etc/resolv.cfg");
}

/**
 * Parses the output of winipcfg or ipconfig.
 */
private void
findWin(InputStream in) {
	String packageName = ResolverConfig.class.getPackage().getName();
	String resPackageName = packageName + ".windows.DNSServer";
	ResourceBundle res = ResourceBundle.getBundle(resPackageName);

	String host_name = res.getString("host_name");
	String primary_dns_suffix = res.getString("primary_dns_suffix");
	String dns_suffix = res.getString("dns_suffix");
	String dns_servers = res.getString("dns_servers");

	BufferedReader br = new BufferedReader(new InputStreamReader(in));
	try {
		List lserver = new ArrayList();
		List lsearch = new ArrayList();
		String line = null;
		boolean readingServers = false;
		boolean readingSearches = false;
		while ((line = br.readLine()) != null) {
			StringTokenizer st = new StringTokenizer(line);
			if (!st.hasMoreTokens()) {
				readingServers = false;
				readingSearches = false;
				continue;
			}
			String s = st.nextToken();
			if (line.indexOf(":") != -1) {
				readingServers = false;
				readingSearches = false;
			}
			
			if (line.indexOf(host_name) != -1) {
				while (st.hasMoreTokens())
					s = st.nextToken();
				Name name;
				try {
					name = Name.fromString(s, null);
				}
				catch (TextParseException e) {
					continue;
				}
				if (name.labels() == 1)
					continue;
				addSearch(s, lsearch);
			} else if (line.indexOf(primary_dns_suffix) != -1) {
				while (st.hasMoreTokens())
					s = st.nextToken();
				if (s.equals(":"))
					continue;
				addSearch(s, lsearch);
				readingSearches = true;
			} else if (readingSearches ||
				   line.indexOf(dns_suffix) != -1)
			{
				while (st.hasMoreTokens())
					s = st.nextToken();
				if (s.equals(":"))
					continue;
				addSearch(s, lsearch);
				readingSearches = true;
			} else if (readingServers ||
				   line.indexOf(dns_servers) != -1)
			{
				while (st.hasMoreTokens())
					s = st.nextToken();
				if (s.equals(":"))
					continue;
				addServer(s, lserver);
				readingServers = true;
			}
		}
		
		if (servers == null && lserver.size() > 0)
			servers = (String [])lserver.toArray
						(new String[lserver.size()]);

		if (searchlist == null && lsearch.size() > 0)
			searchlist =
			    (Name [])lsearch.toArray(new Name[lsearch.size()]);
	}
	catch (IOException e) {
	}
	finally {
		try {
			br.close();
		}
		catch (IOException e) {
		}
	}
	return;
}

/**
 * Calls winipcfg and parses the result to find servers and a search path.
 */
private void
find95() {
	String s = "winipcfg.out";
	try {
		Process p;
		p = Runtime.getRuntime().exec("winipcfg /all /batch " + s);
		p.waitFor();
		File f = new File(s);
		findWin(new FileInputStream(f));
		new File(s).delete();
	}
	catch (Exception e) {
		return;
	}
}

/**
 * Calls ipconfig and parses the result to find servers and a search path.
 */
private void
findNT() {
	try {
		Process p;
		p = Runtime.getRuntime().exec("ipconfig /all");
		findWin(p.getInputStream());
		p.destroy();
	}
	catch (Exception e) {
		return;
	}
}

/** Returns all located servers */
public String []
servers() {
	return servers;
}

/** Returns the first located server */
public String
server() {
	if (servers == null)
		return null;
	return servers[0];
}

/** Returns all entries in the located search path */
public Name []
searchPath() {
	return searchlist;
}

/** Gets the current configuration */
public static synchronized ResolverConfig
getCurrentConfig() {
	return currentConfig;
}

/** Gets the current configuration */
public static synchronized void
refresh() {
	currentConfig = new ResolverConfig();
}

}
