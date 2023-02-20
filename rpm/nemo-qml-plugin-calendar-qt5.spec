Name:       nemo-qml-plugin-calendar-qt5

Summary:    Calendar plugin for Nemo Mobile
Version:    0.6.53
Release:    1
License:    BSD
URL:        https://github.com/sailfishos/nemo-qml-plugin-calendar
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Concurrent)
BuildRequires:  pkgconfig(libmkcal-qt5) >= 0.7.4
BuildRequires:  pkgconfig(KF5CalendarCore)
BuildRequires:  pkgconfig(accounts-qt5)

%description
%{summary}.

%package tests
Summary:    QML calendar plugin tests
BuildRequires:  pkgconfig(Qt5Test)
Requires:   %{name} = %{version}-%{release}
Requires:   blts-tools

%description tests
%{summary}.

%package lightweight
Summary:    Calendar lightweight QML plugin
BuildRequires:  pkgconfig(Qt5DBus)

%description lightweight
%{summary}.

%package tools
Summary:    Calendar import/export tool
License:    BSD
BuildRequires:  pkgconfig(libmkcal-qt5)
BuildRequires:  pkgconfig(KF5CalendarCore)

%description tools
%{summary}.

%prep
%setup -q -n %{name}-%{version}

%build

%qmake5 

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake_install

%files
%defattr(-,root,root,-)
%license LICENSE.BSD
%{_libdir}/qt5/qml/org/nemomobile/calendar/libnemocalendar.so
%{_libdir}/qt5/qml/org/nemomobile/calendar/plugins.qmltypes
%{_libdir}/qt5/qml/org/nemomobile/calendar/qmldir

%files tests
%defattr(-,root,root,-)
/opt/tests/nemo-qml-plugins-qt5/calendar/*

%files lightweight
%defattr(-,root,root,-)
%attr(2755, root, privileged) %{_bindir}/calendardataservice
%{_datadir}/dbus-1/services/org.nemomobile.calendardataservice.service
%{_libdir}/qt5/qml/org/nemomobile/calendar/lightweight/libnemocalendarwidget.so
%{_libdir}/qt5/qml/org/nemomobile/calendar/lightweight/plugins.qmltypes
%{_libdir}/qt5/qml/org/nemomobile/calendar/lightweight/qmldir

%files tools
%defattr(-,root,root,-)
%{_bindir}/icalconverter
