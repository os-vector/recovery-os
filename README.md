# oskr-unlock

This code can create a full OSKR or dev unlock OTA.

## Prerequisites

- Linux of some sort (must be x86_64/amd64)
- Docker

## Create an OSKR unlock OTA

1.

```
git clone https://github.com/DDLbots/vector-oskr-unlock --depth=1
```

2.

```
cd vector-oskr-unlock
sudo ./unlock.sh -bt <dev/oskr> -ap <ABOOT-pw> -op <OTA-pw> -bp <OSKR-boot-passwd> -q <QSN> -e <ESN>
# dev doesn't need a -bp
```

3. Your final OTA should be at `~/vector-oskr-unlock/final-unlock-otas/<esn>.ota`

## How to get a QSN

1. Head to [https://www.project-victor.org/noflow-devsetup](https://www.project-victor.org/noflow-devsetup) and pair with the robot.

2. In the console, enter `logs`

3. Once the logs are downloaded, extract them.

4. In the extracted logs, find and open `factory/log1` in a text editor.

5. The last line in that file will have the QSN and ESN.

## How to apply an OSKR OTA

1. Your Vector must be on a normal firmware, like 2.0.1.6078.

2. Your OTA must be hosted at a file server accessible on the network. It doesn't contain any sensitive data, so it can be public. I usually use `python3 -m http.server`.

3. Head to [https://www.project-victor.org/noflow-devsetup](https://www.project-victor.org/noflow-devsetup) and pair with the robot.

4. In the console, enter `ota-start http://fileserverURL:port/<esn>.ota`
  
 - example for my case: `ota-start http://192.168.1.50:8000/00601b50.ota`

