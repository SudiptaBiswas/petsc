#!/usr/bin/env python
import action
import bs
import fileset
import logging
import transform

import os
import string
import types

class Process (action.Action):
  def __init__(self, products, tag, sources, compiler, compilerFlags, setwiseExecute, updateType = 'immediate'):
    if setwiseExecute:
      action.Action.__init__(self, compiler,     sources, compilerFlags, setwiseExecute)
    else:
      action.Action.__init__(self, self.process, sources, compilerFlags, setwiseExecute)
    self.compiler        = compiler
    self.buildProducts   = 0
    if isinstance(products, fileset.FileSet):
      self.products      = products
    else:
      self.products      = fileset.FileSet()
    self.tag             = tag
    self.updateType      = updateType
    if updateType == 'deferred':
      self.deferredUpdates = fileset.FileSet(tag = 'update '+self.tag)
      if isinstance(self.products, fileset.FileSet):
        self.products = [self.products, self.deferredUpdates]
      else:
        self.products.append(self.deferredUpdates)

  def shellSetAction(self, set):
    if set.tag == self.tag:
      self.debugPrint(self.program+' processing '+self.debugFileSetStr(set), 3, 'compile')
      action.Action.shellSetAction(self, set)
      if self.updateType == 'immediate':
        for file in set:
          bs.sourceDB.updateSource(file)
      elif self.updateType == 'deferred':
        set.tag = 'update '+set.tag
        if isinstance(self.products, fileset.FileSet):
          self.products = [self.products]
        self.products.append(set)
    elif set.tag == 'old '+self.tag:
      pass
    else:
      if isinstance(self.products, fileset.FileSet):
        self.products = [self.products]
      self.products.append(set)

  def process(self, source):
    self.debugPrint(self.compiler+' processing '+source, 3, 'compile')
    # Compile file
    command  = self.compiler
    command += ' '+self.constructFlags(source, self.flags)
    command += ' '+source
    output   = self.executeShellCommand(command, self.errorHandler)
    # Update source DB if it compiled successfully
    if self.updateType == 'immediate':
      bs.sourceDB.updateSource(source)
    elif self.updateType == 'deferred':
      self.deferredUpdates.append(source)
    return source

  def setExecute(self, set):
    if set.tag == self.tag:
      transform.Transform.setExecute(self, set)
    else:
      if isinstance(self.products, fileset.FileSet):
        self.products = [self.products]
      self.products.append(set)

class Compile (action.Action):
  def __init__(self, library, tag, sources, compiler, compilerFlags, archiver, archiverFlags, setwiseExecute, updateType = 'immediate'):
    if setwiseExecute:
      action.Action.__init__(self, self.compileSet, sources, compilerFlags, setwiseExecute)
    else:
      action.Action.__init__(self, self.compile,    sources, compilerFlags, setwiseExecute)
    self.tag           = tag
    self.compiler      = compiler
    self.archiver      = archiver
    self.archiverFlags = archiverFlags
    self.archiverRoot  = ''
    self.library       = library
    self.buildProducts = 0
    self.products      = self.library
    self.updateType    = updateType
    if updateType == 'deferred':
      self.deferredUpdates = fileset.FileSet(tag = 'update '+self.tag)
      if isinstance(self.products, fileset.FileSet):
        self.products = [self.products, self.deferredUpdates]
      else:
        self.products.append(self.deferredUpdates)
    self.includeDirs   = []
    self.defines       = []
    self.rebuildAll    = 0

  def checkIncludeDirectory(self, dirname):
    if not os.path.isdir(dirname):
      raise RuntimeError('Include directory '+dirname+' does not exist')

  def getIncludeFlags(self):
    flags = ''
    for dirname in self.includeDirs:
      self.checkIncludeDirectory(dirname)
      flags += ' -I'+dirname
    return flags

  def getDefines(self):
    flags = ''
    for define in self.defines:
      if type(define) == types.TupleType:
        flags += ' -D'+define[0]+'='+define[1]
      else:
        flags += ' -D'+define
    return flags

  def getLibraryName(self, source = None):
    return self.library[0]

  def compile(self, source):
    library = self.getLibraryName(source)
    self.debugPrint('Compiling '+source+' into '+library, 3, 'compile')
    # Compile file
    command  = self.compiler
    flags    = self.constructFlags(source, self.flags)+self.getDefines()+self.getIncludeFlags()
    command += ' '+flags
    object   = self.getIntermediateFileName(source)
    if (object): command += ' -o '+object
    command += ' '+source
    output   = self.executeShellCommand(command, self.errorHandler)
    # Update source DB if it compiled successfully
    if self.updateType == 'immediate':
      bs.sourceDB.updateSource(source)
    elif self.updateType == 'deferred':
      self.deferredUpdates.append(source)
    # Archive file
    if self.archiver:
      command = self.archiver+' '+self.archiverFlags+' '+library+' '+object
      output  = self.executeShellCommand(command, self.errorHandler)
      os.remove(object)
    return object

  def compileSet(self, set):
    if len(set) == 0: return set
    library = self.getLibraryName()
    self.debugPrint('Compiling '+self.debugFileSetStr(set)+' into '+library, 3, 'compile')
    # Compile files
    command  = self.compiler
    flags    = self.constructFlags(set, self.flags)+self.getDefines()+self.getIncludeFlags()
    command += ' '+flags
    for source in set:
      command += ' '+source
    output   = self.executeShellCommand(command, self.errorHandler)
    # Update source DB if it compiled successfully
    for source in set:
      if self.updateType == 'immediate':
        bs.sourceDB.updateSource(source)
      elif self.updateType == 'deferred':
        self.deferredUpdates.append(source)
    # Archive files
    if self.archiver:
      objects = []
      for source in set:
        objects.append(self.getIntermediateFileName(source))
      command = self.archiver+' '+self.archiverFlags+' '+library
      if self.archiverRoot:
        dirs = string.split(self.archiverRoot, os.sep)
        for object in objects:
          odirs = string.split(object, os.sep)
          if not dirs == odirs[:len(dirs)]: raise RuntimeError('Invalid object '+object+' not under root '+self.archiverRoot)
          command += ' -C '+self.archiverRoot+' '+string.join(odirs[len(dirs):], os.sep)
      else:
        for object in objects:
          command += ' '+object
      output  = self.executeShellCommand(command, self.errorHandler)
      for object in objects:
        os.remove(object)
    return set

  def setExecute(self, set):
    if set.tag == self.tag:
      transform.Transform.setExecute(self, set)
    elif set.tag == 'old '+self.tag:
      if self.rebuildAll: transform.Transform.setExecute(self, set)
    else:
      if isinstance(self.products, fileset.FileSet):
        self.products = [self.products]
      self.products.append(set)

  def setAction(self, set):
    if set.tag == self.tag:
      self.func(set)
    elif set.tag == 'old '+self.tag:
      if self.rebuildAll: self.func(set)
    else:
      if isinstance(self.products, fileset.FileSet):
        self.products = [self.products]
      self.products.append(set)

  def execute(self):
    library = self.getLibraryName()
    if library:
      if not os.path.exists(library):
        self.rebuildAll = 1
        if bs.sourceDB.has_key(library): del bs.sourceDB[library]
        (dir, file) = os.path.split(library)
        if not os.path.exists(dir):
          os.makedirs(dir)
      elif not bs.sourceDB.has_key(library):
        self.rebuildAll = 1
    return action.Action.execute(self)

class TagC (transform.GenericTag):
  def __init__(self, tag = 'c', ext = 'c', sources = None, extraExt = 'h', root = None):
    transform.GenericTag.__init__(self, tag, ext, sources, extraExt, root)

class CompileC (Compile):
  def __init__(self, library, sources = None, tag = 'c', compiler = 'gcc', compilerFlags = '-g -Wall -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-align -Wwrite-strings -Wconversion -Wsign-compare -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn -Wredundant-decls -Wnested-externs -Winline', archiver = 'ar', archiverFlags = 'crv'):
    Compile.__init__(self, library, tag, sources, compiler, '-c '+compilerFlags, archiver, archiverFlags, 0)
    self.includeDirs.append('.')
    self.errorHandler = self.handleCErrors

  def handleCErrors(self, command, status, output):
    if status:
      raise RuntimeError('Could not execute \''+command+'\': '+output)
    elif output.find('warning') >= 0:
      print('\''+command+'\': '+output)

class CompilePythonC (CompileC):
  def __init__(self, sources = None, tag = 'c', compiler = 'gcc', compilerFlags = '-g -Wall -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-align -Wwrite-strings -Wconversion -Wsign-compare -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn -Wredundant-decls -Wnested-externs -Winline', archiver = 'ar', archiverFlags = 'crv'):
    CompileC.__init__(self, None, sources, tag, compiler, compilerFlags, archiver, archiverFlags)
    self.products = []

  def getLibraryName(self, source = None):
    if not source: return None
    (dir, file) = os.path.split(source)
    (base, ext) = os.path.splitext(file)
    if not base[-7:] == '_Module' or not ext == '.c':
      # Stupid Babel hack
      if base == 'SIDLObjA' or base == 'SIDLPyArrays':
        package     = base
        libraryName = os.path.join(dir, package+'.a')
      else:
        raise RuntimeError('Invalid Python extension module: '+source)
    else:
      package     = base[:-7]
      libraryName = os.path.join(dir, package+'module.a')
    return libraryName

  def setExecute(self, set):
    Compile.setExecute(self, set)
    if set.tag == self.tag or set.tag == 'old '+self.tag:
      for source in set:
        self.products.append(fileset.FileSet([self.getLibraryName(source)]))

class TagCxx (transform.GenericTag):
  def __init__(self, tag = 'cxx', ext = 'cc', sources = None, extraExt = 'hh', root = None):
    transform.GenericTag.__init__(self, tag, ext, sources, extraExt, root)

class CompileCxx (Compile):
  def __init__(self, library, sources = None, tag = 'cxx', compiler = 'g++', compilerFlags = '-g -Wall -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-align -Wwrite-strings -Wconversion -Wsign-compare -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn -Wnested-externs -Winline', archiver = 'ar', archiverFlags = 'crv'):
    Compile.__init__(self, library, tag, sources, compiler, '-c '+compilerFlags, archiver, archiverFlags, 0)
    self.includeDirs.append('.')
    self.errorHandler = self.handleCxxErrors

  def handleCxxErrors(self, command, status, output):
    if status:
      raise RuntimeError('Could not execute \''+command+'\': '+output)
    elif output.find('warning') >= 0:
      print('\''+command+'\': '+output)

class TagF77 (transform.GenericTag):
  def __init__(self, tag = 'f77', ext = 'f', sources = None, extraExt = '', root = None):
    transform.GenericTag.__init__(self, tag, ext, sources, extraExt, root)

class CompileF77 (Compile):
  def __init__(self, library, sources = None, tag = 'f77', compiler = 'g77', compilerFlags = '-g', archiver = 'ar', archiverFlags = 'crv'):
    Compile.__init__(self, library, tag, sources, compiler, '-c '+compilerFlags, archiver, archiverFlags, 0)

class TagF90 (transform.GenericTag):
  def __init__(self, tag = 'f90', ext = 'f90', sources = None, extraExt = '', root = None):
    transform.GenericTag.__init__(self, tag, ext, sources, extraExt, root)

class CompileF90 (Compile):
  def __init__(self, library, sources = None, tag = 'f90', compiler = 'f90', compilerFlags = '-g', archiver = 'ar', archiverFlags = 'crv'):
    Compile.__init__(self, library, tag, sources, compiler, '-c '+compilerFlags, archiver, archiverFlags, 0)

class TagJava (transform.GenericTag):
  def __init__(self, tag = 'java', ext = 'java', sources = None, extraExt = '', root = None):
    transform.GenericTag.__init__(self, tag, ext, sources, extraExt, root)

class CompileJava (Compile):
  def __init__(self, library, sources = None, tag = 'java', compiler = 'javac', compilerFlags = '-g', archiver = 'jar', archiverFlags = 'cf'):
    Compile.__init__(self, library, tag, sources, compiler, compilerFlags, archiver, archiverFlags, 1, 'deferred')
    self.includeDirs.append('.')
    self.errorHandler = self.handleJavaErrors

  def getDefines(self):
    return ''

  def checkJavaInclude(self, dirname):
    ext = os.path.splitext(dirname)[1]
    if ext == '.jar':
      if not os.path.isfile(dirname):
        raise RuntimeError('Jar file '+dirname+' does not exist')
    else:
      self.checkIncludeDirectory(dirname)

  def getIncludeFlags(self):
    flags = ' -classpath '
    for dirname in self.includeDirs:
      self.checkJavaInclude(dirname)
      flags += dirname+":"
    return flags[:-1]

  def getIntermediateFileName(self, source):
    (base, ext) = os.path.splitext(source)
    if not ext == '.java': raise RuntimeError('Invalid Java file: '+source)
    return base+'.class'

  def handleJavaErrors(self, command, status, output):
    if status:
      raise RuntimeError('Could not execute \''+command+'\': '+output)
    elif output.find('warning') >= 0:
      print('\''+command+'\': '+output)

  def execute(self):
    retval = Compile.execute(self)
    # Need to update JAR files since they do not get linked
    library = self.getLibraryName()
    bs.sourceDB.updateSource(library)
    return retval

class TagEtags (transform.GenericTag):
  def __init__(self, tag = 'etags', ext = ['c', 'h', 'cc', 'hh', 'py'], sources = None, extraExt = '', root = None):
    transform.GenericTag.__init__(self, tag, ext, sources, extraExt, root)

class CompileEtags (Compile):
  def __init__(self, tagsFile, sources = None, tag = 'etags', compiler = 'etags', compilerFlags = '-a'):
    Compile.__init__(self, tagsFile, tag, sources, compiler, compilerFlags, '', '', 1, 'deferred')

  def constructFlags(self, source, baseFlags):
    return baseFlags+' -f '+self.getLibraryName()

  def getIntermediateFileName(self, source):
    return None
