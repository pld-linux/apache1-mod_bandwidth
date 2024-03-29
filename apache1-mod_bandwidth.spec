%define		mod_name	bandwidth
%define 	apxs		/usr/sbin/apxs1
Summary:	Apache module: bandwidth limits
Summary(pl.UTF-8):	Moduł do Apache: limity pasma
Name:		apache1-mod_%{mod_name}
Version:	2.0.5
Release:	1.5
License:	Apache
Group:		Networking/Daemons
Source0:	ftp://ftp.cohprog.com/pub/apache/module/1.3.0/mod_bandwidth.c
# Source0-md5:	00f0905d777f79485beb428b53191ecf
# ftp://ftp.cohprog.com/pub/apache/module/cleanlink.pl
# modified to work from cron NOT as daemon!
Source1:	%{name}-cleanlink.pl
Source2:	%{name}.conf
# http://www.cohprog.com/v3/bandwidth/doc-en.html
Source4:	%{name}-doc.html
URL:		http://www.cohprog.com/v3/bandwidth/intro-en.html
BuildRequires:	apache1-devel >= 1.3.39
BuildRequires:	rpmbuild(macros) >= 1.268
Requires:	apache1(EAPI)
Requires:	crondaemon
Requires:	procps
Obsoletes:	apache-mod_bandwidth <= 2.0.5
BuildRoot:	%{tmpdir}/%{name}-%{version}-root-%(id -u -n)

%define		_pkglibdir	%(%{apxs} -q LIBEXECDIR 2>/dev/null)
%define		_sysconfdir	%(%{apxs} -q SYSCONFDIR 2>/dev/null)

%description
"Mod_bandwidth" is a module for the Apache webserver that enable the
setting of server-wide or per connection bandwidth limits, based on
the directory, size of files and remote IP/domain.

%description -l pl.UTF-8
Moduł pozwalający na ograniczanie pasma poprzez serwer Apache bazując
na katalogu, wielkości plików oraz zdalnym IP/domenie.

%prep
%setup -q -T -c
cp %{SOURCE0} .

%build
%{apxs} -c mod_%{mod_name}.c -o mod_%{mod_name}.so

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT{%{_pkglibdir},%{_sysconfdir}/conf.d} \
	$RPM_BUILD_ROOT%{_var}/run/%{name}/{link,master} \
	$RPM_BUILD_ROOT{/etc/cron.d,%{_sbindir}}

install mod_%{mod_name}.so $RPM_BUILD_ROOT%{_pkglibdir}
install %{SOURCE1} $RPM_BUILD_ROOT%{_sbindir}
install %{SOURCE2} $RPM_BUILD_ROOT%{_sysconfdir}/conf.d/90_mod_%{mod_name}.conf
install %{SOURCE4} .

echo '* * * * * http %{_sbindir}/%{name}-cleanlink.pl' > \
	$RPM_BUILD_ROOT/etc/cron.d/%{name}

%clean
rm -rf $RPM_BUILD_ROOT

%post
%service -q apache restart

%postun
if [ "$1" = "0" ]; then
	%service -q apache restart
fi

%files
%defattr(644,root,root,755)
%doc *html
%attr(640,root,root) %config(noreplace) %verify(not md5 mtime size) %{_sysconfdir}/conf.d/*_mod_%{mod_name}.conf
%config(noreplace) %verify(not md5 mtime size) %attr(640,root,root) /etc/cron.d/%{name}
%attr(755,root,root) %{_sbindir}/*
%attr(755,root,root) %{_pkglibdir}/*
%attr(750,http,root) %dir %{_var}/run/%{name}
%attr(750,http,root) %dir %{_var}/run/%{name}/link
%attr(750,http,root) %dir %{_var}/run/%{name}/master
