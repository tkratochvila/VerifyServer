//+*****************************************************************************
//                         Honeywell Proprietary
// This document and all information and expression contained herein are the
// property of Honeywell International Inc., are loaned in confidence, and
// may not, in whole or in part, be used, duplicated or disclosed for any
// purpose without prior written permission of Honeywell International Inc.
//               This document is an unpublished work.
//
// Copyright (C) 2021 Honeywell International Inc. All rights reserved.
//+*****************************************************************************
/* 
 * File:   Program.cs
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Threading.Tasks;

namespace CurlClient
{
    public class OSLCFile
    {
        List<string> contributors;
        DateTime created;
        List<string> creators;
        string description;
        string identifier;
        DateTime modified;
        List<string> types;
        string title;
        string instanceShape;
        List<string> serviceProvider;
    }

    public class OSLCAutomationPlan : OSLCFile
    {

        List<string> subjects;

        public List<string> parameterDefinition;
        List<string> usesExecutionEnvironment;
        List<string> futureAction;
        public List<string> inputParameter;
        public string tool;

        public OSLCAutomationPlan() {
            parameterDefinition = new List<string>();
            inputParameter = new List<string>();
        }

        string build()
        {
            string pd = "";

            foreach (string s in parameterDefinition)
            {
                pd +="<oslc_auto:parameterDefinition rdf:ID=\"link1\" " +
                "rdf:resource=\"" + s + "\"/>\n";
            }
            string ip = "";

            foreach (string s in inputParameter)
            {
                ip += "<oslc_auto:inputParameter rdf:ID=\"link1\" " +
                "rdf:resource=\"" + s + "\"/>\n";
            }
            return
                "<rdf:RDF\n" +
                "xmlns:dcterms=\"http://purl.org/dc/terms/\"\n" +
                "xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n" +
                "xmlns:oslc_auto=\"http://open-services.net/ns/auto#\">\n\n" +
                "<oslc_auto:AutomationPlan rdf:about=\"http://example.com/results/4321\">\n" +
                "<oslc_auto:toolName rdf:ID=\"link1\" " +
                "rdf:resource=\"" + tool + "\"/>\n" +
                pd +
                ip +
                "</oslc_auto:AutomationPlan>" +
                "</rdf:RDF>";
        }

        public void add_parameter(string p)
        {
            parameterDefinition.Add(p);
        }

        public void write(string fileName)
        {
            File.WriteAllText(fileName, build());
        }
    }

    class Program
    {
        static String fileName;
        static String execCode;

        public static string Reverse(string s)
        {
            char[] charArray = s.ToCharArray();
            Array.Reverse(charArray);
            return new string(charArray);
        }

        /// <summary>
        /// 
        /// </summary>
        /// 
        static async void UploadModel()
        {
            // ... Use HttpClient.
            var client = new HttpClient();
            client.BaseAddress = new Uri("http://158.138.138.152:6000");
            var request = new HttpRequestMessage(HttpMethod.Post, "");
            request.Headers.Add("type", "upload");
            var content = new MultipartFormDataContent();
            var path = @"C:\code\ForMeth\honeywell_sw\ForReq\Examples\pf_mon_mac.cpp";
        
            //string assetName = Path.GetFileName(path);
            FileStream fs = File.OpenRead(path);

            var streamContent = new StreamContent(fs);
            streamContent.Headers.Add("Content-Type", "text/x-c");
            //streamContent.Headers.Add("type", "update");
            //Content-Disposition: form-data; name="file"; filename="C:\B2BAssetRoot\files\596090\596090.1.mp4";
            streamContent.Headers.Add("Content-Disposition", "form-data; name=\"file\"; filename=\"" + Path.GetFileName(path) + "\"");
            content.Add(streamContent, "file", Path.GetFileName(path));

            request.Content = content;

            String result = "";
            try
            {
                //Console.WriteLine("Sending...");
                var response = await client.SendAsync(request);
                //Console.WriteLine("Got response...");
                var responseContent = response.Content;
                //var foo = response.Headers;
                //Console.WriteLine("Reading...");
                result = await responseContent.ReadAsStringAsync();
                fileName = result.Substring(result.IndexOf(':') + 1);
                //Console.WriteLine("Finished...");
            }
            catch (Exception e)
            {
                Console.WriteLine(e.InnerException.Message);
            }
            Console.WriteLine(result);
        }

        static async void VerifyByDivine()
        {
            // ... Use HttpClient.
            var client = new HttpClient();
            client.BaseAddress = new Uri("http://158.138.138.152:6000");
            var request = new HttpRequestMessage(HttpMethod.Post, "");
            request.Headers.Add("type", "verify");
            var content = new MultipartFormDataContent();
            //var formData = new List<KeyValuePair<string, string>>();
            //formData.Add(new KeyValuePair<string, string>("type", "update"));
            //content.Add(new FormUrlEncodedContent(formData));
            OSLCAutomationPlan ap = new OSLCAutomationPlan();
            ap.parameterDefinition.Add("compile -f -c -std=c++11");
            ap.inputParameter.Add(fileName);
            ap.tool = "divine";
            var path = @"C:\Users\H177213\Downloads\temp.xml";
            ap.write(path);
            //string assetName = Path.GetFileName(path);
            FileStream fs = File.OpenRead(path);
            
                        var streamContent = new StreamContent(fs);
                        streamContent.Headers.Add("Content-Type", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
                        //streamContent.Headers.Add("type", "update");
                        //Content-Disposition: form-data; name="file"; filename="C:\B2BAssetRoot\files\596090\596090.1.mp4";
                        streamContent.Headers.Add("Content-Disposition", "form-data; name=\"file\"; filename=\"" + Path.GetFileName(path) + "\"");
                        content.Add(streamContent, "file", Path.GetFileName(path));
                        
            request.Content = content;

            string result = "";
            try
            {
                //Console.WriteLine("Sending...");
                var response = await client.SendAsync(request);
                //Console.WriteLine("Got response...");
                var responseContent = response.Content;
                //Console.WriteLine("Reading...");
                result = await responseContent.ReadAsStringAsync();
                //Console.WriteLine("Finished...");
                execCode = result.Substring(result.IndexOf("n.") + 3);
            } catch(Exception e)
            {
                Console.WriteLine(e.InnerException.Message);
            }
            Console.WriteLine(result);
        }

        static async void GetResult()
        {

            // ... Use HttpClient.
            var client = new HttpClient();
            client.BaseAddress = new Uri("http://158.138.138.152:6000");
            var request = new HttpRequestMessage(HttpMethod.Post, "");
            request.Headers.Add("type", "monitor");
            request.Headers.Add("id", execCode);
            
            string result = "";
            try
            {
                var response = await client.SendAsync(request);
                var content = response.Content;
                result = await content.ReadAsStringAsync();
            }
            catch (Exception e)
            {
                Console.WriteLine(e.InnerException.Message);
            }
            Console.WriteLine(result);
            // ... Display the result.
        }

        static void Main()
        {
            Task t = new Task(UploadModel);
            t.Start();
            Console.WriteLine("Upload to Rak...");
            Console.ReadLine();
            Task t2 = new Task(VerifyByDivine);
            t2.Start();
            Console.WriteLine($"Verify file { fileName } by Rak...");
            Console.ReadLine();
            
            string q;
            do
            {
                Task t3 = new Task(GetResult);
                t3.Start();
                Console.WriteLine($"Get result of verification n. { execCode } from Rak...");
                q = Console.ReadLine();
            } while (!q.Contains("q"));
        }
    }
}
