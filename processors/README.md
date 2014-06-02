Pipeline applications:

* **first_processor** --- receives documents from crawler and passes them to processors
* **html_processor** --- extracts text from html document
* **feature_extractor** --- extracts features from text
* **last_processor** --- puts processed documents and features into index

Pipeline can be uploaded into cocaine cloud using following scripts:

Step by step tutorial:

1. Install cocaine
2. Build wookie
3. Download [Scripts for uploading wookie pipeline to cocaine](https://gist.github.com/IIoTeP9HuY/d32694581e1247221351) and put them into some folder.
4. Run elliptics
5. Run cocaine
6. Upload all pipeline processors into cocaine:
```bash
upload_all.sh $PATH_TO_FOLDER_WITH_PROCESSORS_BINARIES
```
7. Try test application:
```bash
wookie_feed_pipeline --url http://company.yandex.ru --remote $ELLIPTICS_REMOTE
```

