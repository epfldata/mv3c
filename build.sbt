// --------- project informations
Seq(
  name := "DistributedDBtoaster",
  organization := "ch.epfl.data",
  version := "0.1"
)

// --------- Paths
Seq(
  scalaSource in Compile <<= baseDirectory / "src",
  javaSource in Compile <<= baseDirectory / "src",
  sourceDirectory in Compile <<= baseDirectory / "src",
  scalaSource in Test <<= baseDirectory / "test",
  javaSource in Test <<= baseDirectory / "test",
  sourceDirectory in Test <<= baseDirectory / "test",
  resourceDirectory in Compile <<= baseDirectory / "conf"
)

// --------- Dependencies
// Storm dependencies
/*
resolvers ++= Seq(
  "clojars" at "http://clojars.org/repo/",
  "clojure-releases" at "http://build.clojure.org/releases"
)
*/
libraryDependencies <++= scalaVersion(v=>Seq(
  //"org.spark-project" %% "spark-core"   % "0.8.0-SNAPSHOT",
  "com.typesafe.akka" %% "akka-actor"   % "2.2.0",
  "com.typesafe.akka" %% "akka-remote"  % "2.2.0",
  "org.scala-lang"     % "scala-actors" % v,
  "org.scalatest"     %% "scalatest"    % "2.0.M5b" % "test"
  //"com.github.velvia" %% "scala-storm"  % "0.2.3-SNAPSHOT",
  //"storm"              % "storm"        % "0.8.2"
))

// --------- Compilation options
Seq(
  scalaVersion := "2.10.2",
  scalacOptions ++= Seq("-deprecation", "-unchecked", "-feature", "-optimise", "-Yinline-warnings") // ,"-target:jvm-1.7"
)

Seq(
  autoCompilerPlugins := true,
  libraryDependencies <+= scalaVersion(v=>compilerPlugin("org.scala-lang.plugins" % "continuations" % v)),
  scalacOptions += "-P:continuations:enable"
)

// --------- Execution options
Seq(
  fork := true,
  javaOptions ++= Seq("-Xss128m") // ,"-Xss512m","-Xmx2G","-Xms2G","-XX:MaxPermSize=2G" //,"-verbose:gc"
)

// --------- Custom tasks
addCommandAlias("toast", ";run-main ddbt.Compiler ")

addCommandAlias("check", ";run-main ddbt.UnitTest ")

addCommandAlias("queries", ";run-main ddbt.UnitTest -dtiny -dtiny_del -dstandard -dstandard_del;test-only ddbt.test.gen.*")

addCommandAlias("bench", ";test:run-main ddbt.test.Benchmark ")

TaskKey[Unit]("pkg") <<= classDirectory /*fullClasspath*/ in Compile map { cd =>
  val dir=new java.io.File("lib"); if (!dir.exists) dir.mkdirs;
  scala.sys.process.Process(Seq("jar","-cMf",dir.getPath()+"/ddbt.jar","-C",cd.toString,"ddbt/lib")).!
}

// --------- LMS conditional inclusion
{
  // set ddbt.lms = 1 in conf/ddbt.properties to enable LMS
  val prop = new java.util.Properties()
  try { prop.load(new java.io.FileInputStream("conf/ddbt.properties")) } catch { case _:Throwable => }
  val lms = prop.getProperty("ddbt.lms","0")=="1"
  if (!lms) Seq() else Seq(
    sources in Compile ~= (_ filter (x=> !x.toString.endsWith("codegen/LMSGen.scala"))),
    unmanagedSourceDirectories in Compile += file("lms"),
    // LMS-specific options
    scalaOrganization := "org.scala-lang.virtualized",
    scalaVersion := "2.10.1",
    libraryDependencies ++= Seq(
      "org.scala-lang.virtualized" % "scala-library" % "2.10.1",
      "org.scala-lang.virtualized" % "scala-compiler" % "2.10.1",
      "EPFL" % "lms_2.10" % "0.3-SNAPSHOT"
    ),
    scalacOptions ++= List("-Yvirtualize")
  ) 
}

//{
//  val t=TaskKey[Unit]("queries-gen2")
//  val q=TaskKey[Unit]("queries-test2") 
//  Seq(
//    fullRunTask(t in Compile, Compile, "ddbt.UnitTest", "tiny","tiny_del","standard","standard_del"),
//    q <<= (t in Compile) map { x => scala.sys.process.Process(Seq("sbt", "test-only ddbt.test.gen.*")).!; }
//    //(test in Test) <<= (test in Test) dependsOn (t in Compile),   // force tests to rebuild
//    //testOptions in Test += Tests.Argument("-l", "ddbt.SlowTest"), // execute only non-tagged tests
//  )
//}
// jar -cf foo.jar -C target/scala-2.10/classes ddbt/lib
// TaskKey[Unit]("test-queries") := { scala.sys.process.Process(Seq("sbt", "test-only ddbt.test.gen.*")).! }
//
// http://grokbase.com/t/gg/simple-build-tool/133xb2khew/sbt-external-process-syntax-in-build-sbt
// http://stackoverflow.com/questions/15494508/bash-vs-scala-sys-process-process-with-command-line-arguments
//compile in Compile <<= (compile in Compile) map { x => ("src/librna/make target/scala-2.10/classes").run.exitValue; x }
//	"org.scala-lang" % "scala-reflect" % v,
//	"ch.epfl" %% "lms" % "0.4-SNAPSHOT",
//	"org.scalariform" %% "scalariform" % "0.1.4",
//parallelExecution in Test := false
//scalaOrganization := "org.scala-lang.virtualized"
//scalaVersion := Option(System.getenv("SCALA_VIRTUALIZED_VERSION")).getOrElse("2.10.2-RC1")
//resolvers ++= Seq(
//    ScalaToolsSnapshots, 
//    "Sonatype Public" at "https://oss.sonatype.org/content/groups/public",
//	Classpaths.typesafeSnapshots
//)
// disable publishing of main docs
//publishArtifact in (Compile, packageDoc) := false
// continuations
//autoCompilerPlugins := true
//libraryDependencies <<= (scalaVersion, libraryDependencies) { (ver, deps) =>
//    deps :+ compilerPlugin("org.scala-lang.plugins" % "continuations" % ver)
//}
//mainClass := Some("Main")
//selectMainClass := Some("Main")
