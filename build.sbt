// --------- Project informations
Seq(
  name := "TStore",
  organization := "ch.epfl.data",
  version := "0.1"
)

mainClass in run in Compile := Some("benchmark.OltpBenchmark")

packSettings

packMain := Map("compiler" -> "benchmark.OltpBenchmark")

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
libraryDependencies ++= Seq(
  "org.scala-lang"     % "scala-actors"   % scalaVersion.value, // to compile legacy Scala
  "org.scala-lang"     % "scala-compiler" % scalaVersion.value,
  "org.scalatest"     %% "scalatest"      % "2.2.4" % "test",
  "org.testng"         % "testng"         % "6.9.4" % "test"
)

// --------- Compilation options
Seq(
  scalaVersion := "2.11.6",
  scalacOptions ++= Seq("-deprecation","-unchecked","-feature","-optimise","-Yinline-warnings"), // ,"-target:jvm-1.7"
  javacOptions ++= Seq("-Xlint:unchecked","-Xlint:-options","-source","1.8","-target","1.8") // forces JVM 1.6 compatibility with JDK 1.7 compiler
)

// --------- Execution options
Seq(
  fork := true, // required to enable javaOptions
  //javaOptions ++= Seq("-agentpath:"+"/Applications/Tools/YourKit Profiler.app/bin/mac/libyjpagent.jnilib"+"=sampling,onexit=snapshot,builtinprobes=all"),
  javaOptions ++= Seq("-Xss128m","-XX:-DontCompileHugeMethods","-XX:+CMSClassUnloadingEnabled"), // ,"-Xss512m","-XX:MaxPermSize=2G"
  javaOptions ++= Seq("-Xmx8G","-Xms8G"/*,"-verbose:gc"*/), parallelExecution in Test := false, // for large benchmarks
  javacOptions ++= Seq("-source", "1.8", "-target", "1.8", "-Xlint"),
  javaOptions <+= (fullClasspath in Runtime) map (cp => "-Dsbt.classpath="+cp.files.absString) // propagate paths
)

// --------- Custom tasks
val numWarehouse = "1"

lazy val reloadDB = taskKey[Unit]("Executes the shell script for reloading the database")

reloadDB := {
  "bench/bench load "+numWarehouse !
}

addCommandAlias("bench", ";run-main ddbt.tpcc.tx.TpccInMem -w "+numWarehouse) ++
addCommandAlias("bench-1", ";bench -i -1 -t 60") ++
addCommandAlias("bench1",  ";bench -i 1 -t 60 ") ++
addCommandAlias("bench2",  ";bench -i 2 -t 60 ") ++
addCommandAlias("bench3",  ";bench -i 3 -t 60 ") ++
addCommandAlias("bench4",  ";bench -i 4 -t 60 ") ++
addCommandAlias("bench5",  ";bench -i 5 -t 60 ") ++
addCommandAlias("bench6",  ";bench -i 6 -t 60 ") ++
addCommandAlias("bench7",  ";bench -i 7 -t 60 ") ++
addCommandAlias("bench8",  ";bench -i 8 -t 60 ") ++
addCommandAlias("bench9",  ";bench -i 9 -t 60 ") ++
addCommandAlias("bench10", ";bench -i 10 -t 60 ")

addCommandAlias("unit", ";run-main ddbt.tpcc.loadtest.TpccUnitTest -w "+numWarehouse+" ;reloadDB") ++
addCommandAlias("unit-1", ";unit -i -1") ++
addCommandAlias("unit1",  ";unit -i 1") ++
addCommandAlias("unit2",  ";unit -i 2") ++
addCommandAlias("unit3",  ";unit -i 3") ++
addCommandAlias("unit4",  ";unit -i 4") ++
addCommandAlias("unit5",  ";unit -i 5") ++
addCommandAlias("unit6",  ";unit -i 6") ++
addCommandAlias("unit7",  ";unit -i 7") ++
addCommandAlias("unit8",  ";unit -i 8") ++
addCommandAlias("unit9",  ";unit -i 9") ++
addCommandAlias("unit10", ";unit -i 10")

addCommandAlias("test-mvconcurrent", ";test-only ddbt.tpcc.lib.mvconcurrent.*")

// --------- LMS codegen, enabled with ddbt.lms = 1 in conf/config.properties
{
  val prop=new java.util.Properties(); try { prop.load(new java.io.FileInputStream("conf/config.properties")) } catch { case _:Throwable => }
  if (prop.getProperty("ddbt.lms","0")!="1") Seq() else Seq(
    // assemblyOption in assembly ~= { _.copy(includeScala = false) }
    sources in Compile ~= (fs => fs.filter(f=> !f.toString.endsWith("codegen/LMSGen.scala"))), // ++ (new java.io.File("lms") ** "*.scala").get
    scalaSource in Compile <<= baseDirectory / "lms", // incorrect; but fixes dependency and allows incremental compilation (SBT 0.13.0)
    //unmanagedSourceDirectories in Compile += file("lms"),
    // LMS-specific options
    scalaOrganization := "org.scala-lang.virtualized",
    scalaVersion := "2.10.2-RC1",
    libraryDependencies ++= Seq(
      "com.typesafe.akka" %% "akka-actor"     % "2.2.3",
      "com.typesafe.akka" %% "akka-remote"    % "2.2.3",
      "org.scala-lang.virtualized" % "scala-library" % scalaVersion.value,
      "org.scala-lang.virtualized" % "scala-compiler" % scalaVersion.value,
      "org.apache.logging.log4j" % "log4j-api" % "2.0-rc1",
      "org.apache.logging.log4j" % "log4j-core" % "2.0-rc1",
      "org.slf4j" % "slf4j-api" % "1.7.2",
      "org.slf4j" % "slf4j-ext" % "1.7.2",
      "mysql" % "mysql-connector-java" % "5.1.28",
      "org.scalariform" %% "scalariform" % "0.1.4",
      "org.scalatest" %% "scalatest" % "2.0",
      "EPFL" %% "lms" % "0.3-SNAPSHOT"
    ),
    scalacOptions ++= List("-Yvirtualize")
  )
}
