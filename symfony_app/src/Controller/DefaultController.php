<?php

namespace App\Controller;

use Symfony\Bundle\FrameworkBundle\Controller\AbstractController;
use Symfony\Component\DependencyInjection\ParameterBag\ParameterBagInterface;
use Symfony\Component\HttpFoundation\JsonResponse;
use Symfony\Component\HttpFoundation\Request;
use Symfony\Component\HttpFoundation\Response;
use Symfony\Component\Routing\Annotation\Route;
use Symfony\Contracts\HttpClient\HttpClientInterface;

class DefaultController extends AbstractController
{
    /** @var HttpClientInterface */
    protected $client;

    /** @var string */
    protected $filename;

    /** @var string */
    protected $arduinoIp = "192.168.1.188"; // You can change this as needed

    public function __construct(HttpClientInterface $client, ParameterBagInterface $parameterBag)
    {
        $this->client = $client;
        $this->filename = sprintf("%s/public/songs/current.json", $parameterBag->get('kernel.project_dir'));
    }

    /**
     * @Route("/update-sensor", name="default")
     */
    public function index(Request $request): Response
    {
        $readings = json_decode($request->getContent(), true);

        if (file_exists($this->filename) && filesize($this->filename) > 0) {
            $jsonObj = json_decode(file_get_contents($this->filename), true);
            $existingReading = $jsonObj['readings'];
            foreach ($readings['readings'] as $reading) {
                $existingReading[] = $reading;
            }

            $jsonObj['readings'] = $existingReading;
            $file = fopen($this->filename, "w");
            fwrite($file, json_encode($jsonObj));
        } else {
            $file = fopen($this->filename, "w");
            fwrite($file, json_encode($readings));
        }

        return new JsonResponse("OK", Response::HTTP_OK);
    }

    /**
     * @param Request $request
     * @return Response
     *
     * @Route("/view", name="view")
     */
    public function viewCurrent(Request $request): Response
    {
        return $this->render('default/view.html.twig', []);
    }

    /**
     * @param Request $request
     * @return Response
     *
     * @Route ("/current-json", name="get_readings")
     */
    public function getJson(Request $request): Response
    {
        return new JsonResponse(file_get_contents($this->filename));
    }

    /**
     * @param Request $request
     * @return Response
     *
     * @Route("/admin", name="admin")
     */
    public function admin(Request $request): Response
    {
        return $this->render('default/admin.html.twig', [
        ]);
    }

    /**
     * @return Response
     *
     * @Route("/admin/start", name="startSong")
     */
    public function startSong(): Response
    {
        $response = $this->client->request(
            'GET',
            sprintf('http://%s/start', $this->arduinoIp)
        );

        $statusCode = $response->getStatusCode();

        return new JsonResponse("OK", $statusCode);
    }

    /**
     * @return Response
     *
     * @Route("/admin/stop", name="stopSong")
     */
    public function stopSong(): Response
    {
        $response = $this->client->request(
            'GET',
            sprintf('http://%s/stop', $this->arduinoIp)
        );

        $statusCode = $response->getStatusCode();

        return new JsonResponse("OK", $statusCode);
    }
}
