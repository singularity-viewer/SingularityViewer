#!/usr/bin/python
# @file viewer_manifest.py
# @author Ryan Williams
# @brief Description of all installer viewer files, and methods for packaging
#        them into installers for all supported platforms.
#
# $LicenseInfo:firstyear=2006&license=viewergpl$
# 
# Copyright (c) 2006-2009, Linden Research, Inc.
# 
# Second Life Viewer Source Code
# The source code in this file ("Source Code") is provided by Linden Lab
# to you under the terms of the GNU General Public License, version 2.0
# ("GPL"), unless you have obtained a separate licensing agreement
# ("Other License"), formally executed by you and Linden Lab.  Terms of
# the GPL can be found in doc/GPL-license.txt in this distribution, or
# online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
# 
# There are special exceptions to the terms and conditions of the GPL as
# it is applied to this Source Code. View the full text of the exception
# in the file doc/FLOSS-exception.txt in this software distribution, or
# online at
# http://secondlifegrid.net/programs/open_source/licensing/flossexception
# 
# By copying, modifying or distributing this software, you acknowledge
# that you have read and understood your obligations described above,
# and agree to abide by those obligations.
# 
# ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
# WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
# COMPLETENESS OR PERFORMANCE.
# $/LicenseInfo$
import sys
import os.path
import re
import tarfile
viewer_dir = os.path.dirname(__file__)
# add llmanifest library to our path so we don't have to muck with PYTHONPATH
sys.path.append(os.path.join(viewer_dir, '../lib/python/indra/util'))
from llmanifest import LLManifest, main, proper_windows_path, path_ancestors

class ViewerManifest(LLManifest):
    def construct(self):
        super(ViewerManifest, self).construct()
        self.exclude("*.svn*")
        self.path(src="../../scripts/messages/message_template.msg", dst="app_settings/message_template.msg")
        self.path(src="../../etc/message.xml", dst="app_settings/message.xml")

        if self.prefix(src="app_settings"):
            self.exclude("logcontrol.xml")
            self.exclude("logcontrol-dev.xml")
            self.path("*.pem")
            self.path("*.ini")
            self.path("*.xml")
            self.path("*.db2")

            # include the entire shaders directory recursively
            self.path("shaders")
            # ... and the entire windlight directory
            self.path("windlight")
            # ... and the hunspell dictionaries
            self.path("dictionaries")
            self.end_prefix("app_settings")

        if self.prefix(src="character"):
            self.path("*.llm")
            self.path("*.xml")
            self.path("*.tga")
            self.end_prefix("character")

        # Include our fonts
        if self.prefix(src="fonts"):
            self.path("*.ttf")
            self.path("*.txt")
            self.end_prefix("fonts")

        # skins
        if self.prefix(src="skins"):
                self.path("paths.xml")
                # include the entire textures directory recursively
                if self.prefix(src="default/textures"):
                        self.path("*.tga")
                        self.path("*.j2c")
                        self.path("*.jpg")
                        self.path("*.png")
                        self.path("textures.xml")
                        self.end_prefix("default/textures")
                self.path("default/xui/*/*.xml")
                self.path("Default.xml")
                self.path("default/*.xml")
                if self.prefix(src="dark/textures"):
                        self.path("*.tga")
                        self.path("*.j2c")
                        self.path("*.jpg")
                        self.path("*.png")
                        self.path("textures.xml")
                        self.end_prefix("dark/textures")
                self.path("dark.xml")
                self.path("dark/*.xml")
                
                # Local HTML files (e.g. loading screen)
                if self.prefix(src="*/html"):
                        self.path("*.png")
                        self.path("*/*/*.html")
                        self.path("*/*/*.gif")
                        self.end_prefix("*/html")
                self.end_prefix("skins")
        
        # Files in the newview/ directory
        self.path("gpu_table.txt")

    def login_channel(self):
        """Channel reported for login and upgrade purposes ONLY;
        used for A/B testing"""
        # NOTE: Do not return the normal channel if login_channel
        # is not specified, as some code may branch depending on
        # whether or not this is present
        return self.args.get('login_channel')

    def buildtype(self):
        return self.args['buildtype']
    def standalone(self):
        return self.args['standalone'] == "ON"
    def grid(self):
        return self.args['grid']
    def channel(self):
        return self.args['channel']
    def channel_unique(self):
        return self.channel().replace("Second Life", "").strip()
    def channel_oneword(self):
        return "".join(self.channel_unique().split())
    def channel_lowerword(self):
        return self.channel_oneword().lower()
    def viewer_branding_id(self):
        return self.args['branding_id']
    def installer_prefix(self):
        mapping={"secondlife":'SecondLife_',
                 "snowglobe":'Snowglobe_',
                 "singularity":'Singularity_'}
        return mapping[self.viewer_branding_id()]

    def flags_list(self):
        """ Convenience function that returns the command-line flags
        for the grid"""

        # Set command line flags relating to the target grid
        grid_flags = ''
        if not self.default_grid():
            grid_flags = "--grid %(grid)s "\
                         "--helperuri http://preview-%(grid)s.secondlife.com/helpers/" %\
                           {'grid':self.grid()}

        # set command line flags for channel
        channel_flags = ''
        if self.login_channel() and self.login_channel() != self.channel():
            # Report a special channel during login, but use default
            channel_flags = '--channel "%s"' % (self.login_channel())
        else:
            channel_flags = '--channel "%s"' % self.channel()

        # Deal with settings 
        if self.default_channel() and self.default_grid():
            setting_flags = ''
        elif self.default_grid():
            setting_flags = '--settings settings_%s.xml'\
                            % self.channel_lowerword()
        else:
            setting_flags = '--settings settings_%s_%s.xml'\
                            % (self.grid(), self.channel_lowerword())
                                                
        return " ".join((channel_flags, grid_flags, setting_flags)).strip()

class WindowsManifest(ViewerManifest):
    def final_exe(self):
        return 'SingularityViewer.exe'


    def construct(self):
        super(WindowsManifest, self).construct()
        # the final exe is complicated because we're not sure where it's coming from,
        # nor do we have a fixed name for the executable
        self.path(src='%s/secondlife-bin.exe' % self.args['configuration'], dst=self.final_exe())

        # Plugin host application
        self.path(os.path.join(os.pardir,
                               'llplugin', 'slplugin', self.args['configuration'], "SLPlugin.exe"),
                  "SLPlugin.exe")
        
      	# need to get the kdu dll from any of the build directories as well
        #~ try:
            #~ self.path(self.find_existing_file('../llkdu/%s/llkdu.dll' % self.args['configuration'],
                #~ '../../libraries/i686-win32/lib/release/llkdu.dll'), 
                  #~ dst='llkdu.dll')
            #~ pass
        #~ except:
            #~ print "Skipping llkdu.dll"
            #~ pass
        self.path(src="licenses-win32.txt", dst="licenses.txt")

        self.path("featuretable.txt")

        # For spellchecking
        if self.prefix(src=self.args['configuration'], dst=""):
            self.path("libhunspell.dll")
            self.end_prefix()

        # For use in crash reporting (generates minidumps)
        self.path("dbghelp.dll")

        # For using FMOD for sound... DJS
        if self.prefix(src="../../libraries/i686-win32/lib/release", dst=""):
            try:
                self.path("fmod.dll")
                pass
            except:
                print "Skipping fmod.dll - not found"
                pass
            self.end_prefix()

        # For textures
        #if self.prefix(src="../../libraries/i686-win32/lib/release", dst=""):
        #    self.path("openjpeg.dll")
        #    self.end_prefix()

        # Plugins - FilePicker
        if self.prefix(src='../plugins/filepicker/%s' % self.args['configuration'], dst="llplugin"):
            self.path("basic_plugin_filepicker.dll")
            self.end_prefix()

        # Media plugins - QuickTime
        if self.prefix(src='../plugins/quicktime/%s' % self.args['configuration'], dst="llplugin"):
            self.path("media_plugin_quicktime.dll")
            self.end_prefix()

        # Media plugins - WebKit/Qt
        if self.prefix(src='../plugins/webkit/%s' % self.args['configuration'], dst="llplugin"):
            self.path("media_plugin_webkit.dll")
            self.end_prefix()
            
        # For WebKit/Qt plugin runtimes
        if self.prefix(src="../../libraries/i686-win32/lib/release", dst="llplugin"):
            self.path("libeay32.dll")
            self.path("qtcore4.dll")
            self.path("qtgui4.dll")
            self.path("qtnetwork4.dll")
            self.path("qtopengl4.dll")
            self.path("qtwebkit4.dll")
            self.path("qtxmlpatterns4.dll")
            self.path("ssleay32.dll")
            self.end_prefix()

        # For WebKit/Qt plugin runtimes (image format plugins)
        if self.prefix(src="../../libraries/i686-win32/lib/release/imageformats", dst="llplugin/imageformats"):
            self.path("qgif4.dll")
            self.path("qico4.dll")
            self.path("qjpeg4.dll")
            self.path("qmng4.dll")
            self.path("qsvg4.dll")
            self.path("qtiff4.dll")
            self.end_prefix()

        if self.prefix(src="../../libraries/i686-win32/lib/release/codecs", dst="llplugin/codecs"):
            self.path("qcncodecs4.dll")
            self.path("qjpcodecs4.dll")
            self.path("qkrcodecs4.dll")
            self.path("qtwcodecs4.dll")
            self.end_prefix()

        # Get llcommon and deps. If missing assume static linkage and continue.
        if self.prefix(src=self.args['configuration'], dst=""):
          try:
            self.path('llcommon.dll')
          except RuntimeError, err:
            print err.message
            print "Skipping llcommon.dll (assuming llcommon was linked statically)"
          self.end_prefix()
        if self.prefix(src="../../libraries/i686-win32/lib/release", dst=""):
          try:
            self.path('libapr-1.dll')
            self.path('libaprutil-1.dll')
            self.path('libapriconv-1.dll')
          except RuntimeError, err:
            pass
          self.end_prefix()
        
        # For google-perftools tcmalloc allocator.
        self.path("../../libraries/i686-win32/lib/release/libtcmalloc_minimal.dll", dst="libtcmalloc_minimal.dll")
        
        try:
          if self.prefix("../../libraries/i686-win32/lib/release/msvcrt", dst=""):
            self.path("*.dll")
            self.path("*.manifest")
            self.end_prefix()
        except:
          pass
        
        
        # These need to be installed as a SxS assembly, currently a 'private' assembly.
        # See http://msdn.microsoft.com/en-us/library/ms235291(VS.80).aspx
        #~ if self.prefix(src=self.args['configuration'], dst=""):
            #~ if self.args['configuration'] == 'Debug':
                #~ self.path("msvcr80d.dll")
                #~ self.path("msvcp80d.dll")
                #~ self.path("Microsoft.VC80.DebugCRT.manifest")
            #~ else:
                #~ self.path("msvcr80.dll")
                #~ self.path("msvcp80.dll")
                #~ self.path("Microsoft.VC80.CRT.manifest")
            #~ self.end_prefix()

        # The config file name needs to match the exe's name.
        #~ self.path(src="%s/secondlife-bin.exe.config" % self.args['configuration'], dst=self.final_exe() + ".config")

        # Vivox runtimes
        if self.prefix(src="vivox-runtime/i686-win32", dst=""):
            self.path("SLVoice.exe")
            self.path("alut.dll")
            self.path("vivoxsdk.dll")
            self.path("ortp.dll")
            self.path("wrap_oal.dll")
            self.end_prefix()

        # pull in the crash logger and updater from other projects
        self.path(src='../win_crash_logger/%s/windows-crash-logger.exe' % self.args['configuration'], dst="win_crash_logger.exe")
        self.path(src='../win_updater/%s/windows-updater.exe' % self.args['configuration'], dst="updater.exe")


    def nsi_file_commands(self, install=True):
        def wpath(path):
            if path.endswith('/') or path.endswith(os.path.sep):
                path = path[:-1]
            path = path.replace('/', '\\')
            return path

        result = ""
        dest_files = [pair[1] for pair in self.file_list if pair[0] and os.path.isfile(pair[1])]
        # sort deepest hierarchy first
        dest_files.sort(lambda a,b: cmp(a.count(os.path.sep),b.count(os.path.sep)) or cmp(a,b))
        dest_files.reverse()
        out_path = None
        for pkg_file in dest_files:
            rel_file = os.path.normpath(pkg_file.replace(self.get_dst_prefix()+os.path.sep,''))
            installed_dir = wpath(os.path.join('$INSTDIR', os.path.dirname(rel_file)))
            pkg_file = wpath(os.path.normpath(pkg_file))
            if installed_dir != out_path:
                if install:
                    out_path = installed_dir
                    result += 'SetOutPath ' + out_path + '\n'
            if install:
                result += 'File ' + pkg_file + '\n'
            else:
                result += 'Delete ' + wpath(os.path.join('$INSTDIR', rel_file)) + '\n'
        # at the end of a delete, just rmdir all the directories
        if not install:
            deleted_file_dirs = [os.path.dirname(pair[1].replace(self.get_dst_prefix()+os.path.sep,'')) for pair in self.file_list]
            # find all ancestors so that we don't skip any dirs that happened to have no non-dir children
            deleted_dirs = []
            for d in deleted_file_dirs:
                deleted_dirs.extend(path_ancestors(d))
            # sort deepest hierarchy first
            deleted_dirs.sort(lambda a,b: cmp(a.count(os.path.sep),b.count(os.path.sep)) or cmp(a,b))
            deleted_dirs.reverse()
            prev = None
            for d in deleted_dirs:
                if d != prev:   # skip duplicates
                    result += 'RMDir ' + wpath(os.path.join('$INSTDIR', os.path.normpath(d))) + '\n'
                prev = d

        return result

    def package_finish(self):
        # a standard map of strings for replacing in the templates
        substitution_strings = {
            'version' : '.'.join(self.args['version']),
            'version_short' : '.'.join(self.args['version'][:-1]),
            'version_dashes' : '-'.join(self.args['version']),
            'final_exe' : self.final_exe(),
            'grid':self.args['grid'],
            'grid_caps':self.args['grid'].upper(),
            # escape quotes becase NSIS doesn't handle them well
            'flags':self.flags_list().replace('"', '$\\"'),
            'channel':self.channel(),
            'channel_oneword':self.channel_oneword(),
            'channel_unique':self.channel_unique(),
            }

        version_vars = """
        !define INSTEXE  "%(final_exe)s"
        !define VERSION "%(version_short)s"
        !define VERSION_LONG "%(version)s"
        !define VERSION_DASHES "%(version_dashes)s"
        """ % substitution_strings
        installer_file = "Singularity_%(version_short)s_Setup.exe"
        grid_vars_template = """
        OutFile "%(installer_file)s"
        !define VIEWERNAME "Singularity Viewer"
        !define INSTFLAGS "%(flags)s"
        !define INSTNAME   "SingularityViewer"
        !define SHORTCUT   "Singularity Viewer"
        !define URLNAME   "secondlife"
        !define INSTALL_ICON "install_icon_singularity.ico"
        !define UNINSTALL_ICON "install_icon_singularity.ico"
        Caption "Singularity Viewer ${VERSION}"
        """
        if 'installer_name' in self.args:
            installer_file = self.args['installer_name']
        else:
            installer_file = installer_file % substitution_strings
        substitution_strings['installer_file'] = installer_file

        tempfile = "secondlife_setup_tmp.nsi"
        # the following replaces strings in the nsi template
        # it also does python-style % substitution
        self.replace_in("installers/windows/installer_template.nsi", tempfile, {
                "%%VERSION%%":version_vars,
                "%%SOURCE%%":self.get_src_prefix(),
                "%%GRID_VARS%%":grid_vars_template % substitution_strings,
                "%%INSTALL_FILES%%":self.nsi_file_commands(True),
                "%%DELETE_FILES%%":self.nsi_file_commands(False)})

        # We use the Unicode version of NSIS, available from
        # http://www.scratchpaper.com/
        try:
          import _winreg as reg
          NSIS_path = reg.QueryValue(reg.HKEY_LOCAL_MACHINE, r"SOFTWARE\NSIS\Unicode") + '\\makensis.exe'
          self.run_command('"' + proper_windows_path(NSIS_path) + '" ' + self.dst_path_of(tempfile))
        except:
          try:
            NSIS_path = os.environ['ProgramFiles'] + '\\NSIS\\Unicode\\makensis.exe'
            self.run_command('"' + proper_windows_path(NSIS_path) + '" ' + self.dst_path_of(tempfile))
          except:
            NSIS_path = os.environ['ProgramFiles(X86)'] + '\\NSIS\\Unicode\\makensis.exe'
            self.run_command('"' + proper_windows_path(NSIS_path) + '" ' + self.dst_path_of(tempfile))
        # self.remove(self.dst_path_of(tempfile))
        # If we're on a build machine, sign the code using our Authenticode certificate. JC
        sign_py = os.path.expandvars("{SIGN_PY}")
        if sign_py == "" or sign_py == "{SIGN_PY}":
            sign_py = 'C:\\buildscripts\\code-signing\\sign.py'
        if os.path.exists(sign_py):
            self.run_command('python ' + sign_py + ' ' + self.dst_path_of(installer_file))
        else:
            print "Skipping code signing,", sign_py, "does not exist"
        self.created_path(self.dst_path_of(installer_file))
        self.package_file = installer_file


class DarwinManifest(ViewerManifest):
    def construct(self):
        # copy over the build result (this is a no-op if run within the xcode script)
        self.path(self.args['configuration'] + "/" + self.app_name() + ".app", dst="")

        if self.prefix(src="", dst="Contents"):  # everything goes in Contents
            self.path(self.info_plist_name(), dst="Info.plist")

            # copy additional libs in <bundle>/Contents/MacOS/
            self.path("../../libraries/universal-darwin/lib_release/libhunspell-1.2.dylib", dst="MacOS/libhunspell-1.2.dylib")
            self.path("../../libraries/universal-darwin/lib_release/libndofdev.dylib", dst="MacOS/libndofdev.dylib")
            #self.path("../../libraries/universal-darwin/lib_release/libvorbisenc.2.dylib", dst="MacOS/libvorbisenc.2.dylib")
            #self.path("../../libraries/universal-darwin/lib_release/libvorbisfile.3.dylib", dst="MacOS/libvorbisfile.3.dylib")
            #self.path("../../libraries/universal-darwin/lib_release/libvorbis.0.dylib", dst="MacOS/libvorbis.0.dylib")
            #self.path("../../libraries/universal-darwin/lib_release/libogg.0.dylib", dst="MacOS/libogg.0.dylib")

            # most everything goes in the Resources directory
            if self.prefix(src="", dst="Resources"):
                super(DarwinManifest, self).construct()

                if self.prefix("cursors_mac"):
                    self.path("*.tif")
                    self.end_prefix("cursors_mac")

                self.path("licenses-mac.txt", dst="licenses.txt")
                self.path("featuretable_mac.txt")
                self.path("SecondLife.nib")

   			  # SG:TODO
                self.path("../newview/res/singularity.icns", dst="singularity.icns")

                # Translations
                self.path("English.lproj")
                self.path("German.lproj")
                self.path("Japanese.lproj")
                self.path("Korean.lproj")
                self.path("da.lproj")
                self.path("es.lproj")
                self.path("fr.lproj")
                self.path("hu.lproj")
                self.path("it.lproj")
                self.path("nl.lproj")
                self.path("pl.lproj")
                self.path("pt.lproj")
                self.path("ru.lproj")
                self.path("tr.lproj")
                self.path("uk.lproj")
                self.path("zh-Hans.lproj")

                # SLVoice and vivox lols
                self.path("vivox-runtime/universal-darwin/libalut.dylib", "libalut.dylib")
                self.path("vivox-runtime/universal-darwin/libopenal.dylib", "libopenal.dylib")
                self.path("vivox-runtime/universal-darwin/libortp.dylib", "libortp.dylib")
                self.path("vivox-runtime/universal-darwin/libvivoxsdk.dylib", "libvivoxsdk.dylib")
                self.path("vivox-runtime/universal-darwin/SLVoice", "SLVoice")

		self.path("../llcommon/" + self.args['configuration'] + "/libllcommon.dylib", "libllcommon.dylib")
                
                libfile = "lib%s.dylib"
                libdir = "../../libraries/universal-darwin/lib_release"

                for libfile in ("libapr-1.0.3.7.dylib",
                                "libaprutil-1.0.3.8.dylib",
                                "libexpat.0.5.0.dylib"):
                    self.path(os.path.join(libdir, libfile), libfile)

                # For using FMOD for sound...but, fmod is proprietary so some might not use it...
                try:
                    self.path(self.args['configuration'] + "/libfmodwrapper.dylib", "libfmodwrapper.dylib")
                    pass
                except:
                    print "Skipping libfmodwrapper.dylib - not found"
                    pass

                # our apps
                try:
                  self.path("../mac_crash_logger/" + self.args['configuration'] + "/mac-crash-logger.app", "mac-crash-logger.app")
                  self.path("../mac_updater/" + self.args['configuration'] + "/mac-updater.app", "mac-updater.app")
                except:
                	pass

                # plugin launcher
                self.path("../llplugin/slplugin/" + self.args['configuration'] + "/SLPlugin.app", "SLPlugin.app")
                
                # dependencies on shared libs
                mac_crash_logger_res_path = self.dst_path_of("mac-crash-logger.app/Contents/Resources")
                slplugin_res_path = self.dst_path_of("SLPlugin.app/Contents/Resources")
                for libfile in ("libllcommon.dylib",
                                "libapr-1.0.3.7.dylib",
                                "libaprutil-1.0.3.8.dylib",
                                "libexpat.0.5.0.dylib"):
                    target_lib = os.path.join('../../..', libfile)
                    self.run_command("ln -sf %(target)r %(link)r" %
                                     {'target': target_lib,
                                      'link' : os.path.join(slplugin_res_path, libfile)}
                                     )
                    #self.run_command("ln -sf %(target)r %(link)r" %
                    #                 {'target': target_lib,
                    #                  'link' : os.path.join(mac_crash_logger_res_path, libfile)}
                    #                 )

                # plugins
                if self.prefix(src="", dst="llplugin"):
                    self.path("../plugins/filepicker/" + self.args['configuration'] + "/basic_plugin_filepicker.dylib", "basic_plugin_filepicker.dylib")
                    self.path("../plugins/quicktime/" + self.args['configuration'] + "/media_plugin_quicktime.dylib", "media_plugin_quicktime.dylib")
                    self.path("../plugins/webkit/" + self.args['configuration'] + "/media_plugin_webkit.dylib", "media_plugin_webkit.dylib")
                    self.path("../../libraries/universal-darwin/lib_release/libllqtwebkit.dylib", "libllqtwebkit.dylib")

                    self.end_prefix("llplugin")              

                # command line arguments for connecting to the proper grid
                self.put_in_file(self.flags_list(), 'arguments.txt')

                self.end_prefix("Resources")

            self.end_prefix("Contents")

        # NOTE: the -S argument to strip causes it to keep enough info for
        # annotated backtraces (i.e. function names in the crash log).  'strip' with no
        # arguments yields a slightly smaller binary but makes crash logs mostly useless.
        # This may be desirable for the final release.  Or not.
        if self.buildtype().lower()=='release':
            if ("package" in self.args['actions'] or 
                "unpacked" in self.args['actions']):
                self.run_command('strip -S "%(viewer_binary)s"' %
                                 { 'viewer_binary' : self.dst_path_of('Contents/MacOS/'+self.app_name())})

    def app_name(self):
        return "Singularity"
        
    def info_plist_name(self):
        return "Info-Singularity.plist"

    def package_finish(self):
        channel_standin = self.app_name()
        if not self.default_channel_for_brand():
            channel_standin = self.channel()

        imagename=self.installer_prefix() + '_'.join(self.args['version'])

        # See Ambroff's Hack comment further down if you want to create new bundles and dmg
        volname=self.app_name() + " Installer"  # DO NOT CHANGE without checking Ambroff's Hack comment further down

        if self.default_channel_for_brand():
            if not self.default_grid():
                # beta case
                imagename = imagename + '_' + self.args['grid'].upper()
        else:
            # first look, etc
            imagename = imagename + '_' + self.channel_oneword().upper()

        sparsename = imagename + ".sparseimage"
        finalname = imagename + ".dmg"
        # make sure we don't have stale files laying about
        self.remove(sparsename, finalname)

        self.run_command('hdiutil create "%(sparse)s" -volname "%(vol)s" -fs HFS+ -type SPARSE -megabytes 700 -layout SPUD' % {
                'sparse':sparsename,
                'vol':volname})

        # mount the image and get the name of the mount point and device node
        hdi_output = self.run_command('hdiutil attach -private "' + sparsename + '"')
        devfile = re.search("/dev/disk([0-9]+)[^s]", hdi_output).group(0).strip()
        volpath = re.search('HFS\s+(.+)', hdi_output).group(1).strip()

        # Copy everything in to the mounted .dmg

        if self.default_channel_for_brand() and not self.default_grid():
            app_name = self.app_name() + " " + self.args['grid']
        else:
            app_name = channel_standin.strip()

        # Hack:
        # Because there is no easy way to coerce the Finder into positioning
        # the app bundle in the same place with different app names, we are
        # adding multiple .DS_Store files to svn. There is one for release,
        # one for release candidate and one for first look. Any other channels
        # will use the release .DS_Store, and will look broken.
        # - Ambroff 2008-08-20
		# Added a .DS_Store for snowglobe - Merov 2009-06-17
		
		# We have a single branded installer for all snowglobe channels so snowglobe logic is a bit different
        if (self.app_name()=="Snowglobe"):
            dmg_template = os.path.join ('installers', 'darwin', 'snowglobe-dmg')
        else:
            dmg_template = os.path.join(
                'installers', 
                'darwin',
                '%s-dmg' % "".join(self.channel_unique().split()).lower())

        if not os.path.exists (self.src_path_of(dmg_template)):
            dmg_template = os.path.join ('installers', 'darwin', 'release-dmg')

        for s,d in {self.get_dst_prefix():app_name + ".app",
                    os.path.join(dmg_template, "_VolumeIcon.icns"): ".VolumeIcon.icns",
                    os.path.join(dmg_template, "background.jpg"): "background.jpg",
                    os.path.join(dmg_template, "_DS_Store"): ".DS_Store"}.items():
            print "Copying to dmg", s, d
            self.copy_action(self.src_path_of(s), os.path.join(volpath, d))

        # Hide the background image, DS_Store file, and volume icon file (set their "visible" bit)
        self.run_command('SetFile -a V "' + os.path.join(volpath, ".VolumeIcon.icns") + '"')
        self.run_command('SetFile -a V "' + os.path.join(volpath, "background.jpg") + '"')
        self.run_command('SetFile -a V "' + os.path.join(volpath, ".DS_Store") + '"')

        # Create the alias file (which is a resource file) from the .r
        self.run_command('rez "' + self.src_path_of("installers/darwin/release-dmg/Applications-alias.r") + '" -o "' + os.path.join(volpath, "Applications") + '"')

        # Set the alias file's alias and custom icon bits
        self.run_command('SetFile -a AC "' + os.path.join(volpath, "Applications") + '"')

        # Set the disk image root's custom icon bit
        self.run_command('SetFile -a C "' + volpath + '"')

        # Unmount the image
        self.run_command('hdiutil detach -force "' + devfile + '"')

        print "Converting temp disk image to final disk image"
        self.run_command('hdiutil convert "%(sparse)s" -format UDZO -imagekey zlib-level=9 -o "%(final)s"' % {'sparse':sparsename, 'final':finalname})
        # get rid of the temp file
        self.package_file = finalname
        self.remove(sparsename)

class LinuxManifest(ViewerManifest):
    def construct(self):
        super(LinuxManifest, self).construct()
        self.path("licenses-linux.txt","licenses.txt")
        
        self.path("res/"+self.icon_name(),self.icon_name())
        if self.prefix("linux_tools", dst=""):
            self.path("client-readme.txt","README-linux.txt")
            self.path("client-readme-voice.txt","README-linux-voice.txt")
            self.path("client-readme-joystick.txt","README-linux-joystick.txt")
            self.path("wrapper.sh",self.wrapper_name())
            self.path("handle_secondlifeprotocol.sh")
            self.path("register_secondlifeprotocol.sh")
            self.end_prefix("linux_tools")

        # Create an appropriate gridargs.dat for this package, denoting required grid.
        self.put_in_file(self.flags_list(), 'gridargs.dat')

        if self.buildtype().lower()=='release':
            self.path("secondlife-stripped","bin/"+self.binary_name())
            self.path("../linux_crash_logger/linux-crash-logger-stripped","linux-crash-logger.bin")
        else:
            self.path("secondlife-bin","bin/"+self.binary_name())
            self.path("../linux_crash_logger/linux-crash-logger","linux-crash-logger.bin")

        self.path("linux_tools/launch_url.sh","launch_url.sh")
        self.path("../llplugin/slplugin/SLPlugin", "bin/SLPlugin")
        if self.prefix("res-sdl"):
            self.path("*")
            # recurse
            self.end_prefix("res-sdl")

        # plugins
        if self.prefix(src="", dst="bin/llplugin"):
            self.path("../plugins/filepicker/libbasic_plugin_filepicker.so", "libbasic_plugin_filepicker.so")
            self.path("../plugins/webkit/libmedia_plugin_webkit.so", "libmedia_plugin_webkit.so")
            self.path("../plugins/gstreamer010/libmedia_plugin_gstreamer010.so", "libmedia_plugin_gstreamer.so")
            self.end_prefix("bin/llplugin")

        self.path("featuretable_linux.txt")

    def wrapper_name(self):
        return 'singularity'

    def binary_name(self):
        return 'singularity-do-not-run-directly'
    
    def icon_name(self):
        return "singularity_icon.png"

    def package_finish(self):
        if 'installer_name' in self.args:
            installer_name = self.args['installer_name']
        else:
            installer_name_components = [self.installer_prefix(), self.args.get('arch')]
            installer_name_components.extend(self.args['version'])
            installer_name = "_".join(installer_name_components)
            if self.default_channel():
                if not self.default_grid():
                    installer_name += '_' + self.args['grid'].upper()
            else:
                installer_name += '_' + self.channel_oneword().upper()

        if self.args['buildtype'].lower() in ['release', 'releasesse2']:
            print "* Going strip-crazy on the packaged binaries, since this is a RELEASE build"
            # makes some small assumptions about our packaged dir structure
            self.run_command("find %(d)r/bin %(d)r/lib* -type f | xargs --no-run-if-empty strip --strip-unneeded" % {'d': self.get_dst_prefix()} )
            self.run_command("find %(d)r/bin %(d)r/lib* -type f -not -name \\*.so | xargs --no-run-if-empty strip -s" % {'d': self.get_dst_prefix()} )

        # Fix access permissions
        self.run_command("""
                find '%(dst)s' -type d -print0 | xargs -0 --no-run-if-empty chmod 755;
                find '%(dst)s' -type f -perm 0700 -print0 | xargs -0 --no-run-if-empty chmod 0755;
                find '%(dst)s' -type f -perm 0500 -print0 | xargs -0 --no-run-if-empty chmod 0555;
                find '%(dst)s' -type f -perm 0600 -print0 | xargs -0 --no-run-if-empty chmod 0644;
                find '%(dst)s' -type f -perm 0400 -print0 | xargs -0 --no-run-if-empty chmod 0444;
                true""" %  {'dst':self.get_dst_prefix() })
        self.package_file = installer_name + '.tar.bz2'

        # temporarily move directory tree so that it has the right
        # name in the tarfile
        self.run_command("mv '%(dst)s' '%(inst)s'" % {
            'dst': self.get_dst_prefix(),
            'inst': self.build_path_of(installer_name)})
        try:
            # --numeric-owner hides the username of the builder for
            # security etc.
            # I'm leaving this disabled for speed
            #self.run_command("tar -C '%(dir)s' --numeric-owner -cjf "
            #                 "'%(inst_path)s.tar.bz2' %(inst_name)s" % {
            #    'dir': self.get_build_prefix(),
            #    'inst_name': installer_name,
            #    'inst_path':self.build_path_of(installer_name)})
            print ''
        finally:
            self.run_command("mv '%(inst)s' '%(dst)s'" % {
                'dst': self.get_dst_prefix(),
                'inst': self.build_path_of(installer_name)})


class Linux_i686Manifest(LinuxManifest):
    def construct(self):
        super(Linux_i686Manifest, self).construct()

        self.path("../llcommon/libllcommon.so", "lib/libllcommon.so")

        if (not self.standalone()) and self.prefix("../../libraries/i686-linux/lib_release_client", dst="lib"):

            try:
                self.path("libkdu_v42R.so", "libkdu.so")
                pass
            except:
                print "Skipping libkdu_v42R.so - not found"
                pass

            try:
                self.path("libfmod-3.75.so")
                pass
            except:
                print "Skipping libfmod-3.75.so - not found"
                pass

            self.path("libapr-1.so.0")
            self.path("libaprutil-1.so.0")
            self.path("libdb-4.2.so")
            self.path("libcrypto.so.0.9.7")
            self.path("libexpat.so.1")
            self.path("libhunspell-1.2.so.0.0.0", "libhunspell-1.2.so.0")
            self.path("libssl.so.0.9.7")
            #self.path("libuuid.so.1")
            self.path("libSDL-1.2.so.0")
            self.path("libELFIO.so")
            #self.path("libopenjpeg.so.1.3.0", "libopenjpeg.so.1.3")
            self.path("libalut.so")
            self.path("libopenal.so.1")
            self.path("libtcmalloc_minimal.so.0")
            self.path("libtcmalloc_minimal.so.0.2.2")
            self.end_prefix("lib")


            # Vivox runtimes
            if self.prefix(src="vivox-runtime/i686-linux", dst="bin"):
                    self.path("SLVoice")
                    self.end_prefix()
            if self.prefix(src="vivox-runtime/i686-linux", dst="lib"):
                    self.path("libortp.so")
                    self.path("libvivoxsdk.so")
                    self.end_prefix("lib")


class Linux_x86_64Manifest(LinuxManifest):
    def construct(self):
        super(Linux_x86_64Manifest, self).construct()

        self.path("../llcommon/libllcommon.so", "lib64/libllcommon.so")

        if (not self.standalone()) and self.prefix("../../libraries/x86_64-linux/lib_release_client", dst="lib64"):
            self.path("libapr-1.so.0")
            self.path("libaprutil-1.so.0")
            self.path("libdb-4.2.so")
            self.path("libcrypto.so.0.9.8")
            self.path("libexpat.so.1")
            self.path("libhunspell-1.2.so.0.0.0", "libhunspell-1.2.so.0")
            self.path("libssl.so.0.9.8")
            self.path("libuuid.so", "libuuid.so.1")
            self.path("libSDL-1.2.so.0")
            self.path("libELFIO.so")
            self.path("libjpeg.so.7")
            self.path("libpng12.so.0")
            self.path("libopenjpeg.so.2")
            self.path("libxml2.so.2")
            #self.path("libz.so.1") #not needed

            # OpenAL
            self.path("libopenal.so.1")
            self.path("libalut.so.0")

            self.end_prefix("lib64")

            # Vivox runtimes and libs
            if self.prefix(src="vivox-runtime/i686-linux", dst="bin"):
                self.path("SLVoice")
                self.end_prefix("bin")

            if self.prefix(src="vivox-runtime/i686-linux", dst="lib32"):
                #self.path("libalut.so")
                self.path("libortp.so")
                self.path("libvivoxsdk.so")
                self.end_prefix("lib32")

        # 32bit libs needed for voice
        if self.prefix("../../libraries/x86_64-linux/lib_release_client/32bit-compat", dst="lib32"):
            self.path("libalut.so")
            self.path("libidn.so.11")
            self.path("libopenal.so.1")
            # self.path("libortp.so")
            self.path("libuuid.so.1")
            self.end_prefix("lib32")

if __name__ == "__main__":
    main()
