%define		mod_name	bandwidth
%define 	apxs		/usr/sbin/apxs
Summary:	Apache module: bandwidth limits
Summary(pl):	Modu³ do apache: limity pasma
Name:		apache-mod_%{mod_name}
Version:	2.0.3
Release:	1
License:	Apache
Group:		Networking/Daemons
Source0:	ftp://ftp.cohprog.com/pub/apache/module/1.3.0/mod_bandwidth.c
# ftp://ftp.cohprog.com/pub/apache/module/cleanlink.pl
# modified to work from cron NOT as daemon!
Source1:	%{name}-cleanlink.pl
Source2:	%{name}.conf
# http://www.cohprog.com/v3/bandwidth/doc-en.html
Source4:	%{name}-doc.html
URL:		http://www.cohprog.com/v3/bandwidth/intro-en.html
BuildRequires:	apache(EAPI)-devel
Requires(post,preun):	%{apxs}
Requires(post,preun):	grep
Requires(preun):	fileutils
Requires:	apache(EAPI)
Requires:	procps
BuildRoot:	%{tmpdir}/%{name}-%{version}-root-%(id -u -n)

%define		_pkglibdir	%(%{apxs} -q LIBEXECDIR)
%define         _sysconfdir     /etc/httpd

%description
"Mod_bandwidth" is a module for the Apache webserver that enable the
setting of server-wide or per connection bandwidth limits, based on
the directory, size of files and remote IP/domain.

%description -l pl
Modu³ pozawlaj±cy na ograniczanie pasma poprzez serwer Apache bazuj±c
na katalogu, wielko¶ci plików oraz zdalnym IP/domenie.

%prep
%setup -q -T -c
cp %{SOURCE0} .

%build
%{apxs} -c mod_%{mod_name}.c -o mod_%{mod_name}.so

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT{%{_pkglibdir},%{_sysconfdir}} \
	$RPM_BUILD_ROOT%{_var}/run/%{name}/{link,master} \
	$RPM_BUILD_ROOT{/etc/cron.d,%{_sbindir}}

install mod_%{mod_name}.so $RPM_BUILD_ROOT%{_pkglibdir}
install %{SOURCE1} $RPM_BUILD_ROOT%{_sbindir}
install %{SOURCE2} $RPM_BUILD_ROOT/%{_sysconfdir}/mod_%{mod_name}.conf
install %{SOURCE4} .

echo '* * * * * http %{_sbindir}/%{name}-cleanlink.pl' > \
	$RPM_BUILD_ROOT/etc/cron.d/%{name}

%clean
rm -rf $RPM_BUILD_ROOT

%post
%{apxs} -e -a -n %{mod_name} %{_pkglibdir}/mod_%{mod_name}.so 1>&2
if [ -f /etc/httpd/httpd.conf ] && ! grep -q "^Include.*mod_%{mod_name}.conf" /etc/httpd/httpd.conf; then
        echo "Include /etc/httpd/mod_%{mod_name}.conf" >> /etc/httpd/httpd.conf
fi
if [ -f /var/lock/subsys/httpd ]; then
	/etc/rc.d/init.d/httpd restart 1>&2
fi

%preun
if [ "$1" = "0" ]; then
	%{apxs} -e -A -n %{mod_name} %{_pkglibdir}/mod_%{mod_name}.so 1>&2
	umask 027
	grep -v "^Include.*mod_%{mod_name}.conf" /etc/httpd/httpd.conf > \
		/etc/httpd/httpd.conf.tmp
	mv -f /etc/httpd/httpd.conf.tmp /etc/httpd/httpd.conf
	if [ -f /var/lock/subsys/httpd ]; then
		/etc/rc.d/init.d/httpd restart 1>&2
	fi
fi

%files
%defattr(644,root,root,755)
%doc *html
%attr(640,root,root) %config(noreplace) %verify(not size mtime md5) %{_sysconfdir}/mod_*.conf
%config(noreplace) %verify(not size mtime md5) %attr(640,root,root) /etc/cron.d/%{name}
%attr(755,root,root) %{_sbindir}/*
%attr(755,root,root) %{_pkglibdir}/*
%attr(750,http,root) %dir %{_var}/run/%{name}
%attr(750,http,root) %dir %{_var}/run/%{name}/link
%attr(750,http,root) %dir %{_var}/run/%{name}/master
