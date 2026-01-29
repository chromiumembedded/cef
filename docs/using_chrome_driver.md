This page explains how to use ChromeDriver and Selenium to automate CEF-based applications.


***

**Contents**

- [Overview](#overview)
- [Java Tutorial](#java-tutorial)

---

# Overview

ChromeDriver and Selenium are tools for automated testing of Chromium-based applications. The tests themselves can be written in a number of languages including Java, JavaScript and Python. ChromeDriver communicates with the Chromium-based application using the DevTools remote debugging protocol (configured via the `--remote-debugging-port=XXXX` command-line flag passed to the application). Detailed ChromeDriver/Selenium usage information is available here:

  * https://sites.google.com/a/chromium.org/chromedriver/getting-started
  * https://code.google.com/p/selenium/wiki/GettingStarted

# Java Tutorial

This tutorial demonstrates how to control the cefclient sample application using the Java programming language on Windows. Usage on other platforms and with other CEF-based applications is substantially similar.

1\. Install the Java JDK and add its bin directory to your system PATH variable.

2\. Create a directory that will contain all files. This tutorial uses `c:\temp`.

3\. Download ChromeDriver from https://sites.google.com/a/chromium.org/chromedriver/downloads and extract (e.g. `chromedriver_win32.zip` provides `chomedriver.exe`). This tutorial was tested with version 2.14.

4\. Download `selenium-server-standalone-x.y.z.jar` from http://selenium-release.storage.googleapis.com/index.html. This tutorial was tested with version 2.44.0.

5\. Download a CEF binary distribution client from [Downloads](http://magpcss.net/cef_downloads/) and extract (e.g. `cef_binary_3.2171.1979_windows32_client.7z`).

6\. Create `Example.java`:

```cpp
import org.openqa.selenium.By;
import org.openqa.selenium.WebDriver;
import org.openqa.selenium.WebElement;
import org.openqa.selenium.chrome.ChromeDriver;
import org.openqa.selenium.chrome.ChromeOptions;

public class Example  {
  public static void main(String[] args) {
    // Path to the ChromeDriver executable.
    System.setProperty("webdriver.chrome.driver", "c:/temp/chromedriver.exe");

    ChromeOptions options = new ChromeOptions();
    // Path to the CEF executable.
    options.setBinary("c:/temp/cef_binary_3.2171.1979_windows32_client/Release/cefclient.exe");
    // Port to communicate on. Required starting with ChromeDriver v2.41.
    options.addArguments("remote-debugging-port=12345");

    WebDriver driver = new ChromeDriver(options);
    driver.get("http://www.google.com/xhtml");
    sleep(3000);  // Let the user actually see something!
    WebElement searchBox = driver.findElement(By.name("q"));
    searchBox.sendKeys("ChromeDriver");
    searchBox.submit();
    sleep(3000);  // Let the user actually see something!
    driver.quit();
  }

  static void sleep(int time) {
    try { Thread.sleep(time); } catch (InterruptedException e) {}
  }
}
```

Your directory structure should now look similar to this:
```cpp
c:\temp\
  cef_binary_3.2171.1979_windows32_client\
    Release\
      cefclient.exe  (and other files)
  chromedriver.exe
  Example.java
  selenium-server-standalone-2.44.0.jar
```

7\. Compile `Example.java` to generate `Example.class`:

```
> cd c:\temp
> javac -cp ".;*" Example.java
```

8\. Run the example:

```
> cd c:\temp
> java -cp ".;*" Example
```

When the example is run ChromeDriver will:

  * Output to the console:
```
Starting ChromeDriver 2.14.313457 (3d645c400edf2e2c500566c9aa096063e707c9cf) on port 28110
Only local connections are allowed.
```
  * Launch `cefclient.exe`.
  * Submit a Google search for "ChromeDriver".
  * Close `cefclient.exe`.