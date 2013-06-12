# 
# Do NOT Edit the Auto-generated Part!
# Generated by: spectacle version 0.26
# 

Name:       voicecall

# >> macros
# << macros

Summary:    Dialer engine for Nemo Mobile
Version:    0.0.0
Release:    1
Group:      Communications/Telephony
License:    Apache License, Version 2.0
URL:        http://github.com/nemomobile/voicecall
Source0:    %{name}-%{version}.tar.bz2
Source100:  voicecall.yaml
Requires:   voicecall-qt5
BuildRequires:  pkgconfig(QtDeclarative)
Provides:   voicecall-core >= 0.4.9
Provides:   voicecall-libs >= 0.4.9
Obsoletes:   voicecall-core < 0.4.9
Obsoletes:   voicecall-libs < 0.4.9

%description
%{summary}.

%prep
%setup -q -n %{name}-%{version}

# >> setup
# << setup

%build
# >> build pre
# << build pre

%qmake 

make %{?jobs:-j%jobs}

# >> build post
# << build post

%install
rm -rf %{buildroot}
# >> install pre
# << install pre
%qmake_install

# >> install post
# << install post

%files
%defattr(-,root,root,-)
%{_libdir}/qt4/imports/stage/rubyx/voicecall/libvoicecall.so
%{_libdir}/qt4/imports/stage/rubyx/voicecall/qmldir
# >> files
# << files